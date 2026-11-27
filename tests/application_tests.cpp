#include "goodreader/alignment/text_normalizer.hpp"
#include "goodreader/application/find_text_match.hpp"
#include "goodreader/application/local_services.hpp"
#include "goodreader/application/use_cases.hpp"

#include "test_support.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace goodreader::tests {
namespace {

using namespace goodreader::application;
using namespace goodreader::domain;

[[nodiscard]] std::string validation_code(const AppResult<ImportedBook>& result) {
    if (result.has_value()) {
        return {};
    }
    const auto* error = std::get_if<ValidationError>(&result.error());
    return error == nullptr ? std::string{} : error->code;
}

class FakeBookRepository final : public BookRepository {
  public:
    AppResult<std::optional<Book>> find_by_id(const BookId&) override { return find_result; }

    AppResult<std::vector<Book>> search(const std::optional<std::string>& query) override {
        last_query = query;
        return search_result;
    }

    AppResult<Unit> save(const Book&) override { return save_result; }

    AppResult<std::optional<Book>> find_result =
        AppResult<std::optional<Book>>::success(std::nullopt);
    AppResult<std::vector<Book>> search_result = AppResult<std::vector<Book>>::success({});
    AppResult<Unit> save_result = AppResult<Unit>::success(Unit{});
    std::optional<std::string> last_query;
};

class FakeProgressRepository final : public ProgressRepository {
  public:
    AppResult<std::optional<ReadingProgress>> reading_progress(const BookId&) override {
        return reading_result;
    }

    AppResult<Unit> save_reading_progress(const ReadingProgress& value) override {
        reading = value;
        return write_result;
    }

    AppResult<std::optional<ListeningProgress>> listening_progress(const BookId&) override {
        return listening_result;
    }

    AppResult<Unit> save_listening_progress(const ListeningProgress& value) override {
        listening = value;
        return write_result;
    }

    AppResult<std::optional<ReadingProgress>> reading_result =
        AppResult<std::optional<ReadingProgress>>::success(std::nullopt);
    AppResult<std::optional<ListeningProgress>> listening_result =
        AppResult<std::optional<ListeningProgress>>::success(std::nullopt);
    AppResult<Unit> write_result = AppResult<Unit>::success(Unit{});
    std::optional<ReadingProgress> reading;
    std::optional<ListeningProgress> listening;
};

class FakeImporter final : public BookImporter {
  public:
    AppResult<ImportedBook> import(const BookImportRequest& request) override {
        ++calls;
        last_request = request;
        return response;
    }

    AppResult<ImportedBook> response =
        AppResult<ImportedBook>::failure(UnavailableError{"importer", "offline"});
    std::size_t calls = 0;
    std::optional<BookImportRequest> last_request;
};

class FakeConverter final : public BookConverter {
  public:
    AppResult<ConversionProgress> convert(const std::string& source_uri,
                                          ConversionFormat output_format) override {
        ++calls;
        last_uri = source_uri;
        last_format = output_format;
        return response;
    }

    AppResult<ConversionProgress> response =
        AppResult<ConversionProgress>::success(ConversionProgress{0.5});
    std::size_t calls = 0;
    std::string last_uri;
    ConversionFormat last_format = ConversionFormat::epub;
};

class FakeSpeechRecognizer final : public SpeechRecognizer {
  public:
    AppResult<RecognizedSpeech> transcribe(const std::string& audio_uri, std::int64_t start_millis,
                                           std::int64_t duration_millis) override {
        last_uri = audio_uri;
        last_start_millis = start_millis;
        last_duration_millis = duration_millis;
        ++calls;
        return response;
    }

    AppResult<RecognizedSpeech> response =
        AppResult<RecognizedSpeech>::success(RecognizedSpeech{"recognized words", 0, 1000, "fake"});
    std::size_t calls = 0;
    std::string last_uri;
    std::int64_t last_start_millis = -1;
    std::int64_t last_duration_millis = -1;
};

class FakeAuthService final : public AuthService {
  public:
    AppResult<LocalUser> current_user() override { return response; }

    AppResult<LocalUser> response = AppResult<LocalUser>::success(LocalUser{"user", false});
};

class FakeEntitlementService final : public EntitlementService {
  public:
    AppResult<bool> is_enabled(Feature feature) override {
        last_feature = feature;
        return response;
    }

    AppResult<bool> response = AppResult<bool>::success(false);
    Feature last_feature = Feature::book_import;
};

TEST(ApplicationSearchTests, TrimsEmptyQueriesAndPropagatesRepositoryResults) {
    FakeBookRepository books;
    SearchLibraryUseCase search(books);
    EXPECT_TRUE(search("   ").has_value());
    EXPECT_FALSE(books.last_query.has_value());

    const AppError storage_error = UnavailableError{"storage", "offline"};
    books.search_result = AppResult<std::vector<Book>>::failure(storage_error);
    const auto failed = search(" title ");
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error(), storage_error);
    EXPECT_EQ(books.last_query, "title");
}

TEST(ApplicationImportTests, ImportsBooksWithOptionalAudio) {
    FakeImporter importer;
    const auto expected_book =
        Book(BookId("book"), "Title", std::nullopt, BookFormat::epub, "file:///book.epub", 20);
    importer.response = AppResult<ImportedBook>::success(ImportedBook{expected_book, {}});
    ImportBookUseCase import(importer);

    const BookImportRequest request{"file:///book.epub", "epub", {}};
    const auto result = import(request);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().book, expected_book);
    EXPECT_TRUE(result.value().audio_tracks.empty());
    EXPECT_EQ(importer.calls, 1);
    EXPECT_EQ(importer.last_request, request);
}

TEST(ApplicationImportTests, ValidatesBookAudioAndChapterOrderBeforeImport) {
    FakeImporter importer;
    ImportBookUseCase import(importer);

    EXPECT_EQ(validation_code(import({" ", "epub", {}})), "book_uri_required");
    EXPECT_EQ(validation_code(import({"book.txt", "txt", {}})), "unsupported_book_format");
    EXPECT_EQ(validation_code(import({"book.pdf", "pdf", {AudioImportFile{" ", "mp3", 0}}})),
              "unsupported_audio_format");
    EXPECT_EQ(
        validation_code(import({"book.pdf", "pdf", {AudioImportFile{"track.flac", "flac", 0}}})),
        "unsupported_audio_format");
    EXPECT_EQ(validation_code(import(
                  {"book.pdf",
                   "pdf",
                   {AudioImportFile{"one.mp3", "mp3", 0}, AudioImportFile{"two.m4a", "m4a", 0}}})),
              "invalid_audio_order");
    EXPECT_EQ(
        validation_code(import({"book.pdf", "pdf", {AudioImportFile{"track.mp3", "mp3", -1}}})),
        "invalid_audio_order");
    EXPECT_EQ(importer.calls, 0);
}

TEST(ApplicationImportTests, AcceptsAnOrderedBookAndAudioPairAndPropagatesImportErrors) {
    FakeImporter importer;
    const auto track = AudioTrack(AudioTrackId("audio"), BookId("book"), "file:///chapter.m4b",
                                  AudioFormat::m4b, 1000, 0);
    const auto book = Book(BookId("book"), "Title", std::nullopt, BookFormat::pdf,
                           "file:///book.pdf", std::nullopt);
    const ImportedBook imported{book, {track}};
    importer.response = AppResult<ImportedBook>::success(imported);
    const BookImportRequest request{
        "file:///book.pdf", "pdf", {AudioImportFile{"file:///chapter.m4b", "m4b", 0}}};
    const auto result = ImportBookUseCase(importer)(request);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), imported);
    EXPECT_EQ(importer.calls, 1);

    const AppError import_error = UnavailableError{"importer", "unavailable"};
    importer.response = AppResult<ImportedBook>::failure(import_error);
    const auto failed = ImportBookUseCase(importer)(request);
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error(), import_error);
}

TEST(ApplicationProgressTests, SavesReadingAndListeningPositionsAndPreservesStorageErrors) {
    FakeProgressRepository progress;
    const Timestamp timestamp{Milliseconds{1000}};
    const ReadingProgress reading{BookId("book"), 10, timestamp};
    const ListeningProgress listening{BookId("book"), AudioTrackId("track"), 20, timestamp};
    EXPECT_TRUE(SaveReadingProgressUseCase(progress)(reading).has_value());
    EXPECT_TRUE(SaveListeningProgressUseCase(progress)(listening).has_value());
    EXPECT_EQ(progress.reading, reading);
    EXPECT_EQ(progress.listening, listening);

    const AppError error = UnavailableError{"storage", "offline"};
    progress.write_result = AppResult<Unit>::failure(error);
    EXPECT_EQ(SaveReadingProgressUseCase(progress)(reading).error(), error);
    EXPECT_EQ(SaveListeningProgressUseCase(progress)(listening).error(), error);
}

TEST(ApplicationConversionTests, ValidatesSourceAndDelegatesConversion) {
    FakeConverter converter;
    ConvertPdfUseCase convert(converter);
    const auto invalid = convert(" ", ConversionFormat::epub);
    ASSERT_FALSE(invalid.has_value());
    ASSERT_TRUE(std::holds_alternative<ValidationError>(invalid.error()));
    EXPECT_EQ(std::get<ValidationError>(invalid.error()).code, "source_uri_required");
    EXPECT_EQ(converter.calls, 0);

    const auto converted = convert("file:///book.pdf", ConversionFormat::fb2);
    ASSERT_TRUE(converted.has_value());
    EXPECT_DOUBLE_EQ(converted.value().completed, 0.5);
    EXPECT_EQ(converter.last_uri, "file:///book.pdf");
    EXPECT_EQ(converter.last_format, ConversionFormat::fb2);
    EXPECT_EQ(converter.calls, 1);
}

TEST(ApplicationExtensionTests, PropagatesFeatureAndIdentityServices) {
    FakeEntitlementService entitlements;
    CheckFeatureUseCase check(entitlements);
    EXPECT_FALSE(check(Feature::audio_sync).value());
    EXPECT_EQ(entitlements.last_feature, Feature::audio_sync);
    const AppError entitlement_error = UnavailableError{"entitlements", "offline"};
    entitlements.response = AppResult<bool>::failure(entitlement_error);
    EXPECT_EQ(check(Feature::pdf_conversion).error(), entitlement_error);

    FakeAuthService authentication;
    GetCurrentUserUseCase get_user(authentication);
    EXPECT_EQ(get_user().value(), LocalUser("user", false));
    const AppError auth_error = UnavailableError{"authentication", "offline"};
    authentication.response = AppResult<LocalUser>::failure(auth_error);
    EXPECT_EQ(get_user().error(), auth_error);
}

TEST(ApplicationExtensionTests, DefaultsToLocalIdentityAndAllFeaturesUnlocked) {
    LocalAuthService authentication;
    const auto user = authentication.current_user();
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user.value(), LocalUser("local-user", true));

    UnlockedEntitlementService entitlements;
    for (const auto feature :
         {Feature::book_import, Feature::audio_sync, Feature::pdf_conversion}) {
        EXPECT_TRUE(entitlements.is_enabled(feature).value());
    }
}

TEST(ApplicationValueTests, RejectsInvalidPortValues) {
    EXPECT_THROW(RecognizedSpeech(" ", 0, 1, "fake"), std::invalid_argument);
    EXPECT_THROW(RecognizedSpeech("words", -1, 1, "fake"), std::invalid_argument);
    EXPECT_THROW(RecognizedSpeech("words", 1, 1, "fake"), std::invalid_argument);
    EXPECT_THROW(RecognizedSpeech("words", 0, 1, " "), std::invalid_argument);
    EXPECT_THROW(ConversionProgress(-0.1), std::invalid_argument);
    EXPECT_THROW(ConversionProgress(1.1), std::invalid_argument);
    EXPECT_THROW(LocalUser("", true), std::invalid_argument);
}

TEST(FindTextMatchTests, UsesFakeAsrOnTheBoundedFragmentAndMatchesRatioEstimate) {
    const auto book_text = read_fixture("alignment/abridged-book.txt");
    const auto transcript = read_fixture("alignment/abridged-audio.txt");
    const auto book_words = alignment::TextNormalizer::tokenize(book_text);
    const auto spoken_words = alignment::TextNormalizer::tokenize(transcript);
    const auto expected_offset = static_cast<std::int32_t>(
        std::find(book_words.begin(), book_words.end(), spoken_words.front()) - book_words.begin());
    FakeSpeechRecognizer asr;
    asr.response = AppResult<RecognizedSpeech>::success(
        RecognizedSpeech{transcript, 75'000, 100'000, "fake-1"});
    const auto result = FindTextMatchUseCase(asr)(
        book_text, "file:///book.m4b", 100'000, 200'000,
        alignment::AlignmentSearchConfig(0.8, 0.65, 1.8, 0.55, 0.0, 25'000));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(asr.last_uri, "file:///book.m4b");
    EXPECT_EQ(asr.last_start_millis, 75'000);
    EXPECT_EQ(asr.last_duration_millis, 25'000);
    EXPECT_EQ(result.value().fragment_start_millis, 75'000);
    EXPECT_EQ(result.value().transcript.engine_version, "fake-1");
    EXPECT_EQ(result.value().match.word_offset.value, expected_offset);
    EXPECT_GT(result.value().match.confidence, 0.7);
}

TEST(FindTextMatchTests, RejectsInvalidInputBeforeCallingAsr) {
    FakeSpeechRecognizer asr;
    FindTextMatchUseCase find_match(asr);
    const auto blank_audio = find_match("book words", " ", 0, 1000);
    ASSERT_FALSE(blank_audio.has_value());
    EXPECT_EQ(std::get<ValidationError>(blank_audio.error()).code, "audio_uri_required");
    const auto invalid_position = find_match("book words", "audio", 2000, 1000);
    ASSERT_FALSE(invalid_position.has_value());
    EXPECT_EQ(std::get<ValidationError>(invalid_position.error()).code, "invalid_audio_position");
    const auto negative_position = find_match("book words", "audio", -1, 1000);
    EXPECT_EQ(std::get<ValidationError>(negative_position.error()).code, "invalid_audio_position");
    const auto invalid_duration = find_match("book words", "audio", 0, 0);
    EXPECT_EQ(std::get<ValidationError>(invalid_duration.error()).code, "invalid_audio_position");
    const auto empty_book = find_match("!?", "audio", 0, 1000);
    EXPECT_EQ(std::get<ValidationError>(empty_book.error()).code, "empty_book_text");
    EXPECT_EQ(asr.calls, 0);
}

TEST(FindTextMatchTests, TranscribesFromTheBeginningAndUsesTheRemainingTrackDuration) {
    FakeSpeechRecognizer asr;
    asr.response = AppResult<RecognizedSpeech>::success(
        RecognizedSpeech{"the traveler crossed the quiet bridge before dawn", 0, 10'000, "fake"});
    const auto result = FindTextMatchUseCase(asr)(
        "the traveler crossed the quiet bridge before dawn", "audio", 0, 10'000,
        alignment::AlignmentSearchConfig(1.0, 0.65, 1.8, 0.6, 0.0, 25'000));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(asr.last_start_millis, 0);
    EXPECT_EQ(asr.last_duration_millis, 10'000);
}

TEST(FindTextMatchTests, PropagatesAsrFailuresAndRejectsWrongEditions) {
    FakeSpeechRecognizer asr;
    const AppError asr_error = UnavailableError{"asr", "unavailable"};
    asr.response = AppResult<RecognizedSpeech>::failure(asr_error);
    auto find_match = FindTextMatchUseCase(asr);
    const auto transcription_failure = find_match("book words", "audio", 1000, 5000);
    ASSERT_FALSE(transcription_failure.has_value());
    EXPECT_EQ(transcription_failure.error(), asr_error);

    asr.response = AppResult<RecognizedSpeech>::success(
        RecognizedSpeech{read_fixture("alignment/wrong-edition-audio.txt"), 0, 20'000, "fake"});
    const auto wrong_edition =
        find_match("the original edition contains entirely unrelated material", "audio", 0, 100'000,
                   alignment::AlignmentSearchConfig(1.0, 0.65, 1.8, 0.8));
    ASSERT_FALSE(wrong_edition.has_value());
    EXPECT_TRUE(std::holds_alternative<UnavailableError>(wrong_edition.error()));
}

} // namespace
} // namespace goodreader::tests
