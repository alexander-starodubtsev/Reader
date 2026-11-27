#include "goodreader/infrastructure/filesystem_book_importer.hpp"

#include "goodreader/domain/types.hpp"

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace goodreader::infrastructure {
namespace {

using application::AppError;
using application::AppResult;
using application::AudioImportFile;
using application::BookImportRequest;
using application::ImportedBook;
using application::Unit;
using domain::AudioFormat;
using domain::AudioTrack;
using domain::AudioTrackId;
using domain::Book;
using domain::BookFormat;
using domain::BookId;

[[nodiscard]] AppResult<ImportedBook> validation_failure(std::string code, std::string detail) {
    return AppResult<ImportedBook>::failure(
        AppError{application::ValidationError{std::move(code), std::move(detail)}});
}

[[nodiscard]] std::string utf8_from_u8(std::u8string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

[[nodiscard]] std::optional<std::string> decode_file_uri(const std::string& uri) {
    std::string path = uri;
    constexpr std::string_view file_prefix = "file://";
    const bool is_file_uri = path.starts_with(file_prefix);
    if (path.starts_with(file_prefix)) {
        path.erase(0, file_prefix.size());
        constexpr std::string_view localhost_prefix = "localhost/";
        if (path.starts_with(localhost_prefix)) {
            path.erase(0, std::string_view("localhost").size());
        } else if (path.empty() || path.front() != '/') {
            return std::nullopt;
        }
#ifdef _WIN32
        if (path.size() >= 3 && path[0] == '/' &&
            std::isalpha(static_cast<unsigned char>(path[1])) != 0 && path[2] == ':') {
            path.erase(0, 1);
        }
#endif
    } else if (path.find("://") != std::string::npos) {
        return std::nullopt;
    }

    if (!is_file_uri) {
        return path;
    }

    std::string decoded;
    decoded.reserve(path.size());
    for (std::size_t index = 0; index < path.size(); ++index) {
        if (path[index] != '%') {
            decoded.push_back(path[index]);
            continue;
        }
        if (index + 2 >= path.size()) {
            return std::nullopt;
        }
        const auto hex_value = [](char character) -> int {
            if (character >= '0' && character <= '9') {
                return character - '0';
            }
            if (character >= 'a' && character <= 'f') {
                return character - 'a' + 10;
            }
            if (character >= 'A' && character <= 'F') {
                return character - 'A' + 10;
            }
            return -1;
        };
        const auto high = hex_value(path[index + 1]);
        const auto low = hex_value(path[index + 2]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        decoded.push_back(static_cast<char>((high << 4) | low));
        index += 2;
    }
    return decoded;
}

[[nodiscard]] std::filesystem::path local_path_from_uri(const std::string& uri) {
    const auto decoded = decode_file_uri(uri);
    if (!decoded.has_value()) {
        return {};
    }
    std::u8string utf8_path;
    utf8_path.reserve(decoded->size());
    for (const auto character : *decoded) {
        utf8_path.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    }
    return std::filesystem::path(utf8_path);
}

[[nodiscard]] bool is_regular_local_file(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }
    std::error_code error;
    const auto regular = std::filesystem::is_regular_file(path, error);
    return !error && regular;
}

[[nodiscard]] std::optional<BookFormat> format_for_path(const std::filesystem::path& path) {
    return domain::book_format_from_extension(utf8_from_u8(path.extension().u8string()));
}

[[nodiscard]] std::optional<AudioFormat> audio_format_for_path(const std::filesystem::path& path) {
    return domain::audio_format_from_extension(utf8_from_u8(path.extension().u8string()));
}

[[nodiscard]] AppResult<ImportedBook> missing_file(const std::string& kind,
                                                   const std::string& uri) {
    return AppResult<ImportedBook>::failure(AppError{application::NotFoundError{kind, uri}});
}

[[nodiscard]] TagLib::FileRef open_audio_file(const std::filesystem::path& path) {
#ifdef _WIN32
    return TagLib::FileRef(path.c_str());
#else
    const auto native_path = path.string();
    return TagLib::FileRef(native_path.c_str());
#endif
}

[[nodiscard]] std::optional<std::int64_t> audio_duration_millis(const std::filesystem::path& path) {
    auto file = open_audio_file(path);
    if (file.isNull() || file.audioProperties() == nullptr) {
        return std::nullopt;
    }
    const auto duration = file.audioProperties()->lengthInMilliseconds();
    if (duration <= 0) {
        return std::nullopt;
    }
    return duration;
}

[[nodiscard]] AppResult<Unit> validate_audio_file(const AudioImportFile& audio,
                                                  const std::filesystem::path& path,
                                                  std::optional<AudioFormat> requested_format) {
    if (!requested_format.has_value()) {
        return AppResult<Unit>::failure(AppError{application::ValidationError{
            "unsupported_audio_format", "Choose MP3, M4A, M4B, OGG, or WAV audio."}});
    }
    if (!is_regular_local_file(path)) {
        return AppResult<Unit>::failure(
            AppError{application::NotFoundError{"audio_file", audio.uri}});
    }
    if (audio_format_for_path(path) != requested_format) {
        return AppResult<Unit>::failure(AppError{application::ValidationError{
            "audio_format_mismatch",
            "The selected audio file extension does not match its format."}});
    }
    return AppResult<Unit>::success(Unit{});
}

} // namespace

std::string RandomIdGenerator::next_id() {
    std::random_device entropy;
    std::array<unsigned int, 4> words{};
    for (auto& word : words) {
        word = entropy();
    }
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const auto word : words) {
        result << std::setw(8) << word;
    }
    return result.str();
}

FilesystemBookImporter::FilesystemBookImporter(application::LibraryImportRepository& library,
                                               application::IdGenerator& ids)
    : library_(library), ids_(ids) {}

AppResult<ImportedBook> FilesystemBookImporter::import(const BookImportRequest& request) {
    const auto requested_book_format = domain::book_format_from_extension(request.book_extension);
    if (!requested_book_format.has_value()) {
        return validation_failure("unsupported_book_format", "Choose a PDF, EPUB, or FB2 file.");
    }
    const auto book_path = local_path_from_uri(request.book_uri);
    if (book_path.empty()) {
        return validation_failure("local_file_required",
                                  "Selected books must be available as app-owned local files.");
    }
    if (!is_regular_local_file(book_path)) {
        return missing_file("book_file", request.book_uri);
    }
    if (format_for_path(book_path) != requested_book_format) {
        return validation_failure("book_format_mismatch",
                                  "The selected book file extension does not match its format.");
    }

    auto title = utf8_from_u8(book_path.stem().u8string());
    if (title.empty()) {
        title = utf8_from_u8(book_path.filename().u8string());
    }
    if (title.empty()) {
        return validation_failure("book_title_required",
                                  "The selected book has no usable filename.");
    }

    const Book book(BookId(ids_.next_id()), std::move(title), std::nullopt, *requested_book_format,
                    request.book_uri, std::nullopt);
    std::set<std::int32_t> chapters;
    std::vector<AudioTrack> tracks;
    tracks.reserve(request.audio_files.size());
    for (const auto& audio : request.audio_files) {
        const auto requested_audio_format = domain::audio_format_from_extension(audio.extension);
        if (!requested_audio_format.has_value()) {
            return validation_failure("unsupported_audio_format",
                                      "Choose MP3, M4A, M4B, OGG, or WAV audio.");
        }
        if (audio.chapter_index < 0 || !chapters.insert(audio.chapter_index).second) {
            return validation_failure("invalid_audio_order",
                                      "Audio chapter positions must be unique and non-negative.");
        }
        const auto audio_path = local_path_from_uri(audio.uri);
        if (audio_path.empty()) {
            return validation_failure(
                "local_file_required",
                "Selected audio must be available as an app-owned local file.");
        }
        const auto audio_validation =
            validate_audio_file(audio, audio_path, requested_audio_format);
        if (!audio_validation.has_value()) {
            return AppResult<ImportedBook>::failure(audio_validation.error());
        }
        const auto duration = audio_duration_millis(audio_path);
        if (!duration.has_value()) {
            return validation_failure("invalid_audio_file",
                                      "The selected audio file has no readable duration.");
        }
        tracks.emplace_back(AudioTrackId(ids_.next_id()), book.id, audio.uri,
                            *requested_audio_format, *duration, audio.chapter_index);
    }

    std::sort(tracks.begin(), tracks.end(), [](const AudioTrack& left, const AudioTrack& right) {
        return left.chapter_index < right.chapter_index;
    });
    const auto persisted = library_.save_imported_book(book, tracks);
    if (!persisted.has_value()) {
        return AppResult<ImportedBook>::failure(persisted.error());
    }
    return AppResult<ImportedBook>::success(ImportedBook{book, std::move(tracks)});
}

} // namespace goodreader::infrastructure
