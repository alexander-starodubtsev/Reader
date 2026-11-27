#include "goodreader/infrastructure/filesystem_book_importer.hpp"
#include "goodreader/infrastructure/sqlite_library_repository.hpp"

#include <sqlite3.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace goodreader::infrastructure {
namespace {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        static std::atomic_uint64_t counter{0};
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ =
            std::filesystem::temp_directory_path() / ("goodreader-test-" + std::to_string(suffix) +
                                                      "-" + std::to_string(counter.fetch_add(1)));
        std::error_code error;
        if (!std::filesystem::create_directories(path_, error) || error) {
            throw std::runtime_error("Unable to create test directory");
        }
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

class SequentialIdGenerator final : public application::IdGenerator {
  public:
    std::string next_id() override { return "generated-" + std::to_string(next_++); }

  private:
    std::uint32_t next_ = 0;
};

void write_u16(std::ostream& output, std::uint16_t value) {
    output.put(static_cast<char>(value & 0xffU));
    output.put(static_cast<char>((value >> 8U) & 0xffU));
}

void write_u32(std::ostream& output, std::uint32_t value) {
    output.put(static_cast<char>(value & 0xffU));
    output.put(static_cast<char>((value >> 8U) & 0xffU));
    output.put(static_cast<char>((value >> 16U) & 0xffU));
    output.put(static_cast<char>((value >> 24U) & 0xffU));
}

void write_wav(const std::filesystem::path& path) {
    constexpr std::uint32_t sample_rate = 8000;
    constexpr std::uint16_t channels = 1;
    constexpr std::uint16_t bits_per_sample = 16;
    constexpr std::uint32_t data_size = sample_rate * channels * (bits_per_sample / 8U);
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Unable to create WAV fixture");
    }
    output.write("RIFF", 4);
    write_u32(output, 36U + data_size);
    output.write("WAVEfmt ", 8);
    write_u32(output, 16);
    write_u16(output, 1);
    write_u16(output, channels);
    write_u32(output, sample_rate);
    write_u32(output, sample_rate * channels * (bits_per_sample / 8U));
    write_u16(output, channels * (bits_per_sample / 8U));
    write_u16(output, bits_per_sample);
    output.write("data", 4);
    write_u32(output, data_size);
    const std::array<char, 4096> silence{};
    auto remaining = data_size;
    while (remaining > 0) {
        const auto count = static_cast<std::streamsize>(
            std::min<std::uint32_t>(remaining, static_cast<std::uint32_t>(silence.size())));
        output.write(silence.data(), count);
        remaining -= static_cast<std::uint32_t>(count);
    }
    if (!output) {
        throw std::runtime_error("Unable to write WAV fixture");
    }
}

[[nodiscard]] std::string path_string(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return std::string(value.begin(), value.end());
}

[[nodiscard]] std::string error_code(const application::AppError& error) {
    if (const auto* validation = std::get_if<application::ValidationError>(&error)) {
        return validation->code;
    }
    return {};
}

class SqliteLibraryRepositoryTest : public ::testing::Test {
  protected:
    TemporaryDirectory temporary_directory;
    std::unique_ptr<SqliteLibraryRepository> repository;

    void SetUp() override {
        auto opened = SqliteLibraryRepository::open(
            path_string(temporary_directory.path() / "library.sqlite3"));
        ASSERT_TRUE(opened.has_value());
        repository = std::move(opened.value());
    }
};

TEST_F(SqliteLibraryRepositoryTest, MigratesFreshDatabaseToCurrentSchema) {
    sqlite3* database = nullptr;
    ASSERT_EQ(sqlite3_open(path_string(temporary_directory.path() / "library.sqlite3").c_str(),
                           &database),
              SQLITE_OK);
    sqlite3_stmt* statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(database, "PRAGMA user_version", -1, &statement, nullptr),
              SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(statement, 0), SqliteLibraryRepository::current_schema_version);
    sqlite3_finalize(statement);
    sqlite3_close(database);
}

TEST_F(SqliteLibraryRepositoryTest, MigratesVersionOneDataWithoutLosingBooks) {
    repository.reset();
    const auto database_path = path_string(temporary_directory.path() / "legacy.sqlite3");
    sqlite3* database = nullptr;
    ASSERT_EQ(sqlite3_open(database_path.c_str(), &database), SQLITE_OK);
    char* message = nullptr;
    const auto status = sqlite3_exec(
        database,
        "CREATE TABLE Book(id TEXT NOT NULL PRIMARY KEY,title TEXT NOT NULL,author TEXT,"
        "format TEXT NOT NULL,word_count INTEGER);"
        "INSERT INTO Book VALUES('legacy-book','Legacy Book','Author','EPUB',42);"
        "PRAGMA user_version = 1;",
        nullptr, nullptr, &message);
    sqlite3_free(message);
    ASSERT_EQ(status, SQLITE_OK);
    sqlite3_close(database);

    auto opened = SqliteLibraryRepository::open(database_path);
    ASSERT_TRUE(opened.has_value());
    auto migrated = std::move(opened.value());
    const auto book = migrated->find_by_id(domain::BookId("legacy-book"));
    ASSERT_TRUE(book.has_value());
    ASSERT_TRUE(book.value().has_value());
    EXPECT_EQ(book.value()->title, "Legacy Book");
    EXPECT_EQ(book.value()->file_uri, "legacy://unavailable");
}

TEST_F(SqliteLibraryRepositoryTest, SavesImportAndSearchesBooksAndOrderedTracks) {
    const domain::Book book(domain::BookId("book-1"), "The Book", "Ada Author",
                            domain::BookFormat::epub, "/books/the-book.epub", 1234);
    const std::vector<domain::AudioTrack> tracks{
        {domain::AudioTrackId("track-2"), book.id, "/audio/part-2.wav", domain::AudioFormat::wav,
         9000, 1},
        {domain::AudioTrackId("track-1"), book.id, "/audio/part-1.wav", domain::AudioFormat::wav,
         8000, 0}};
    ASSERT_TRUE(repository->save_imported_book(book, tracks).has_value());

    const auto found = repository->find_by_id(book.id);
    ASSERT_TRUE(found.has_value());
    ASSERT_TRUE(found.value().has_value());
    EXPECT_EQ(*found.value(), book);
    const auto searched = repository->search("ada");
    ASSERT_TRUE(searched.has_value());
    ASSERT_EQ(searched.value().size(), 1U);
    EXPECT_EQ(searched.value().front(), book);
    const auto audio_tracks = repository->list_for_book(book.id);
    ASSERT_TRUE(audio_tracks.has_value());
    ASSERT_EQ(audio_tracks.value().size(), 2U);
    EXPECT_EQ(audio_tracks.value()[0].chapter_index, 0);
    EXPECT_EQ(audio_tracks.value()[1].chapter_index, 1);
}

TEST_F(SqliteLibraryRepositoryTest, RollsBackBookReplacementWhenTrackInsertionFails) {
    const domain::Book existing(domain::BookId("existing"), "Original title", std::nullopt,
                                domain::BookFormat::pdf, "/books/original.pdf", std::nullopt);
    const domain::Book other(domain::BookId("other"), "Other", std::nullopt,
                             domain::BookFormat::fb2, "/books/other.fb2", std::nullopt);
    const domain::AudioTrack original_track(domain::AudioTrackId("original-track"), existing.id,
                                            "/audio/original.wav", domain::AudioFormat::wav, 1000,
                                            0);
    const domain::AudioTrack conflicting_track(original_track.id, other.id, "/audio/other.wav",
                                               domain::AudioFormat::wav, 1000, 0);
    ASSERT_TRUE(repository->save_imported_book(existing, {original_track}).has_value());
    ASSERT_TRUE(repository->save_imported_book(other, {}).has_value());

    const domain::Book replacement(existing.id, "Replacement title", std::nullopt,
                                   domain::BookFormat::pdf, "/books/replacement.pdf", std::nullopt);
    EXPECT_FALSE(repository->save_imported_book(replacement, {conflicting_track}).has_value());
    const auto stored = repository->find_by_id(existing.id);
    ASSERT_TRUE(stored.has_value());
    ASSERT_TRUE(stored.value().has_value());
    EXPECT_EQ(stored.value()->title, "Original title");
    const auto tracks = repository->list_for_book(existing.id);
    ASSERT_TRUE(tracks.has_value());
    ASSERT_EQ(tracks.value().size(), 1U);
    EXPECT_EQ(tracks.value().front(), original_track);
}

TEST_F(SqliteLibraryRepositoryTest, PersistsReadingAndListeningProgressSessionsAndAlignment) {
    const domain::Book book(domain::BookId("book"), "Book", std::nullopt, domain::BookFormat::epub,
                            "/books/book.epub", std::nullopt);
    const domain::AudioTrack track(domain::AudioTrackId("track"), book.id, "/audio/book.wav",
                                   domain::AudioFormat::wav, 3000, 0);
    ASSERT_TRUE(repository->save_imported_book(book, {track}).has_value());
    const domain::Timestamp at(domain::Milliseconds(5000));
    const domain::ReadingProgress reading(book.id, 77, at);
    const domain::ListeningProgress listening(book.id, track.id, 1200, at);
    ASSERT_TRUE(repository->save_reading_progress(reading).has_value());
    ASSERT_TRUE(repository->save_listening_progress(listening).has_value());
    const domain::ReadingSession session("session", book.id, "America/Los_Angeles",
                                         {{domain::Timestamp(domain::Milliseconds(1000)),
                                           domain::Timestamp(domain::Milliseconds(9000))}});
    ASSERT_TRUE(repository->save(session).has_value());
    const domain::AlignmentIndex index(
        {{domain::WordOffset(0), 0, 0.9}, {domain::WordOffset(10), 2000, 0.95}});
    ASSERT_TRUE(repository->save(book.id, "fingerprint", index).has_value());

    const auto saved_reading = repository->reading_progress(book.id);
    ASSERT_TRUE(saved_reading.has_value());
    ASSERT_TRUE(saved_reading.value().has_value());
    EXPECT_EQ(*saved_reading.value(), reading);
    const auto saved_listening = repository->listening_progress(book.id);
    ASSERT_TRUE(saved_listening.has_value());
    ASSERT_TRUE(saved_listening.value().has_value());
    EXPECT_EQ(*saved_listening.value(), listening);
    const auto saved_sessions = repository->list_reading_sessions_for_book(book.id);
    ASSERT_TRUE(saved_sessions.has_value());
    ASSERT_EQ(saved_sessions.value().size(), 1U);
    EXPECT_EQ(saved_sessions.value().front().active_millis(), 8000);
    const auto saved_index = repository->find(book.id, "fingerprint");
    ASSERT_TRUE(saved_index.has_value());
    ASSERT_TRUE(saved_index.value().has_value());
    EXPECT_EQ(*saved_index.value(), index);
}

TEST_F(SqliteLibraryRepositoryTest, SearchWithBlankQueryReturnsAllBooks) {
    const domain::Book book(domain::BookId("book"), "Book title", std::nullopt,
                            domain::BookFormat::pdf, "/books/book.pdf", std::nullopt);
    ASSERT_TRUE(repository->save(book).has_value());
    const auto results = repository->search(std::string(" \t "));
    ASSERT_TRUE(results.has_value());
    ASSERT_EQ(results.value().size(), 1U);
}

TEST_F(SqliteLibraryRepositoryTest, ImporterPersistsSupportedBookFormatsAndOptionalAudio) {
    SequentialIdGenerator ids;
    FilesystemBookImporter importer(*repository, ids);
    const std::array<std::string, 3> extensions{"pdf", "epub", "fb2"};
    for (const auto& extension : extensions) {
        const auto book_path = temporary_directory.path() / ("A Book." + extension);
        std::ofstream(book_path).put('x');
        const auto result = importer.import({path_string(book_path), extension, {}});
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value().book.title, "A Book");
        EXPECT_TRUE(result.value().audio_tracks.empty());
        const auto stored = repository->find_by_id(result.value().book.id);
        ASSERT_TRUE(stored.has_value());
        ASSERT_TRUE(stored.value().has_value());
    }
}

TEST_F(SqliteLibraryRepositoryTest, ImporterPairsMultipleAudioFilesInChapterOrder) {
    const auto book_path = temporary_directory.path() / "Audio book.epub";
    const auto first_audio = temporary_directory.path() / "chapter-1.wav";
    const auto second_audio = temporary_directory.path() / "chapter-2.wav";
    std::ofstream(book_path).put('x');
    write_wav(first_audio);
    write_wav(second_audio);
    SequentialIdGenerator ids;
    FilesystemBookImporter importer(*repository, ids);
    const auto result = importer.import(
        {path_string(book_path),
         "epub",
         {{path_string(second_audio), "wav", 1}, {path_string(first_audio), "WAV", 0}}});
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().audio_tracks.size(), 2U);
    EXPECT_EQ(result.value().audio_tracks[0].chapter_index, 0);
    EXPECT_EQ(result.value().audio_tracks[1].chapter_index, 1);
    const auto tracks = repository->list_for_book(result.value().book.id);
    ASSERT_TRUE(tracks.has_value());
    ASSERT_EQ(tracks.value().size(), 2U);
    EXPECT_EQ(tracks.value()[0].chapter_index, 0);
    EXPECT_EQ(tracks.value()[1].chapter_index, 1);
}

TEST_F(SqliteLibraryRepositoryTest, ImporterRejectsMissingFilesAndMismatchedFormats) {
    SequentialIdGenerator ids;
    FilesystemBookImporter importer(*repository, ids);
    const auto missing =
        importer.import({path_string(temporary_directory.path() / "missing.pdf"), "pdf", {}});
    ASSERT_FALSE(missing.has_value());
    EXPECT_TRUE(std::holds_alternative<application::NotFoundError>(missing.error()));

    const auto wrong_extension = temporary_directory.path() / "book.fb2";
    std::ofstream(wrong_extension).put('x');
    const auto mismatch = importer.import({path_string(wrong_extension), "pdf", {}});
    ASSERT_FALSE(mismatch.has_value());
    EXPECT_EQ(error_code(mismatch.error()), "book_format_mismatch");
}

TEST_F(SqliteLibraryRepositoryTest, ImporterRejectsUnreadableAudioWithoutSavingBook) {
    const auto book_path = temporary_directory.path() / "Book.pdf";
    const auto audio_path = temporary_directory.path() / "bad.wav";
    std::ofstream(book_path).put('x');
    std::ofstream(audio_path).put('x');
    SequentialIdGenerator ids;
    FilesystemBookImporter importer(*repository, ids);
    const auto result =
        importer.import({path_string(book_path), "pdf", {{path_string(audio_path), "wav", 0}}});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(error_code(result.error()), "invalid_audio_file");
    const auto books = repository->search(std::nullopt);
    ASSERT_TRUE(books.has_value());
    EXPECT_TRUE(books.value().empty());
}

TEST_F(SqliteLibraryRepositoryTest, RandomIdGeneratorProducesDistinctOpaqueIds) {
    RandomIdGenerator ids;
    const auto first = ids.next_id();
    const auto second = ids.next_id();
    EXPECT_EQ(first.size(), 32U);
    EXPECT_NE(first, second);
}

} // namespace
} // namespace goodreader::infrastructure
