#include "goodreader/application/use_cases.hpp"

#include "goodreader/domain/types.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace goodreader::application {
namespace {

[[nodiscard]] std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\n\r\f\v");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\n\r\f\v");
    return value.substr(first, last - first + 1);
}

[[nodiscard]] bool is_blank(const std::string& value) {
    return value.find_first_not_of(" \t\n\r\f\v") == std::string::npos;
}

[[nodiscard]] AppResult<ImportedBook> validation_failure(std::string code, std::string detail) {
    return AppResult<ImportedBook>::failure(ValidationError{std::move(code), std::move(detail)});
}

} // namespace

SearchLibraryUseCase::SearchLibraryUseCase(BookRepository& books) : books_(books) {}

AppResult<std::vector<domain::Book>>
SearchLibraryUseCase::operator()(std::optional<std::string> query) {
    if (query.has_value()) {
        auto normalized = trim(std::move(*query));
        query =
            normalized.empty() ? std::nullopt : std::optional<std::string>(std::move(normalized));
    }
    return books_.search(query);
}

ImportBookUseCase::ImportBookUseCase(BookImporter& importer) : importer_(importer) {}

AppResult<ImportedBook> ImportBookUseCase::operator()(const BookImportRequest& request) {
    if (is_blank(request.book_uri)) {
        return validation_failure("book_uri_required", "Choose a book file.");
    }
    if (!domain::book_format_from_extension(request.book_extension).has_value()) {
        return validation_failure("unsupported_book_format", "Choose a PDF, EPUB, or FB2 file.");
    }

    std::set<std::int32_t> chapter_indices;
    for (const auto& audio_file : request.audio_files) {
        if (is_blank(audio_file.uri) ||
            !domain::audio_format_from_extension(audio_file.extension).has_value()) {
            return validation_failure("unsupported_audio_format", "Choose supported audio files.");
        }
        if (audio_file.chapter_index < 0 ||
            !chapter_indices.insert(audio_file.chapter_index).second) {
            return validation_failure("invalid_audio_order",
                                      "Audio chapter positions must be unique and non-negative.");
        }
    }
    return importer_.import(request);
}

SaveReadingProgressUseCase::SaveReadingProgressUseCase(ProgressRepository& progress)
    : progress_(progress) {}

AppResult<Unit> SaveReadingProgressUseCase::operator()(const domain::ReadingProgress& progress) {
    return progress_.save_reading_progress(progress);
}

SaveListeningProgressUseCase::SaveListeningProgressUseCase(ProgressRepository& progress)
    : progress_(progress) {}

AppResult<Unit>
SaveListeningProgressUseCase::operator()(const domain::ListeningProgress& progress) {
    return progress_.save_listening_progress(progress);
}

ConvertPdfUseCase::ConvertPdfUseCase(BookConverter& converter) : converter_(converter) {}

AppResult<ConversionProgress> ConvertPdfUseCase::operator()(const std::string& source_uri,
                                                            ConversionFormat output_format) {
    if (is_blank(source_uri)) {
        return AppResult<ConversionProgress>::failure(
            ValidationError{"source_uri_required", "Choose a PDF file."});
    }
    return converter_.convert(source_uri, output_format);
}

CheckFeatureUseCase::CheckFeatureUseCase(EntitlementService& entitlements)
    : entitlements_(entitlements) {}

AppResult<bool> CheckFeatureUseCase::operator()(Feature feature) {
    return entitlements_.is_enabled(feature);
}

GetCurrentUserUseCase::GetCurrentUserUseCase(AuthService& authentication)
    : authentication_(authentication) {}

AppResult<LocalUser> GetCurrentUserUseCase::operator()() { return authentication_.current_user(); }

} // namespace goodreader::application
