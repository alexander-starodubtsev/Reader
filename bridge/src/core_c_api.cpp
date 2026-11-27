#include "goodreader/core_c_api.h"
#include "goodreader/domain/alignment.hpp"
#include "goodreader/reader/document.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

struct operation_state {
    std::atomic_bool cancelled{false};
};

struct core_state {
    std::atomic_bool shutting_down{false};
    std::mutex operations_mutex;
    std::vector<std::weak_ptr<operation_state>> operations;
};

} // namespace

struct gr_core {
    std::shared_ptr<core_state> state;
};

struct gr_book_document {
    goodreader::reader::ReaderDocument value;
};

struct gr_alignment_index {
    goodreader::domain::AlignmentIndex value;
};

struct gr_operation {
    std::shared_ptr<operation_state> state;
};

namespace {

struct validation_result {
    gr_status_t status;
    gr_book_format_t format;
    std::string detail;
};

[[nodiscard]] std::filesystem::path path_from_utf8(const char* path_utf8) {
    std::u8string path;
    for (const auto* character = path_utf8; *character != '\0'; ++character) {
        path.push_back(static_cast<char8_t>(static_cast<unsigned char>(*character)));
    }
    return std::filesystem::path(path);
}

[[nodiscard]] gr_status_t copy_string(const std::string& value, char* destination,
                                      std::size_t capacity, std::size_t* required) {
    if (required == nullptr || (destination == nullptr && capacity > 0)) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    *required = value.size() + 1;
    if (capacity < *required) {
        if (destination != nullptr && capacity > 0)
            destination[0] = '\0';
        return GR_STATUS_BUFFER_TOO_SMALL;
    }
    std::copy(value.begin(), value.end(), destination);
    destination[value.size()] = '\0';
    return GR_STATUS_OK;
}

void copy_error_detail(const std::string& value, char* destination, std::size_t capacity,
                       std::size_t* required) noexcept {
    if (required != nullptr)
        *required = value.size() + 1;
    if (destination == nullptr || capacity == 0)
        return;
    const auto copied = std::min(value.size(), capacity - 1);
    std::copy_n(value.begin(), copied, destination);
    destination[copied] = '\0';
}

[[nodiscard]] gr_book_format_t format_for_path(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    for (auto& character : extension) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    if (extension == ".pdf") {
        return GR_BOOK_FORMAT_PDF;
    }
    if (extension == ".epub") {
        return GR_BOOK_FORMAT_EPUB;
    }
    if (extension == ".fb2") {
        return GR_BOOK_FORMAT_FB2;
    }
    return GR_BOOK_FORMAT_UNKNOWN;
}

[[nodiscard]] validation_result validate_path(const std::string& path_utf8) {
    if (path_utf8.empty()) {
        return {GR_STATUS_INVALID_ARGUMENT, GR_BOOK_FORMAT_UNKNOWN, "The file path is empty."};
    }

    try {
        std::u8string path_string;
        const std::string path_bytes(path_utf8);
        path_string.reserve(path_bytes.size());
        for (const auto character : path_bytes) {
            path_string.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
        }
        const std::filesystem::path path(path_string);
        const auto format = format_for_path(path);
        if (format == GR_BOOK_FORMAT_UNKNOWN) {
            return {GR_STATUS_UNSUPPORTED_FORMAT, GR_BOOK_FORMAT_UNKNOWN,
                    "Choose a PDF, EPUB, or FB2 file."};
        }

        std::error_code error;
        const bool is_regular_file = std::filesystem::is_regular_file(path, error);
        if (error) {
            if (error == std::errc::no_such_file_or_directory) {
                return {GR_STATUS_NOT_FOUND, GR_BOOK_FORMAT_UNKNOWN,
                        "The selected file does not exist."};
            }
            return {GR_STATUS_IO_ERROR, GR_BOOK_FORMAT_UNKNOWN, "The file could not be inspected."};
        }
        if (!is_regular_file) {
            return {GR_STATUS_NOT_FOUND, GR_BOOK_FORMAT_UNKNOWN,
                    "The selected file does not exist."};
        }
        return {GR_STATUS_OK, format, {}};
    } catch (...) {
        return {GR_STATUS_INTERNAL_ERROR, GR_BOOK_FORMAT_UNKNOWN,
                "The file path could not be validated."};
    }
}

[[nodiscard]] gr_status_t validate_arguments(const gr_core_t* core, const char* path_utf8) {
    if (core == nullptr || path_utf8 == nullptr) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    if (core->state->shutting_down.load()) {
        return GR_STATUS_CANCELLED;
    }
    return GR_STATUS_OK;
}

void invoke_callback(const std::shared_ptr<operation_state>& operation,
                     const std::shared_ptr<core_state>& core, const std::string& path,
                     gr_validation_callback_t callback, void* user_data) noexcept {
    if (operation->cancelled.load() || core->shutting_down.load()) {
        try {
            callback(GR_STATUS_CANCELLED, GR_BOOK_FORMAT_UNKNOWN, "The operation was cancelled.",
                     user_data);
        } catch (...) {
        }
        return;
    }

    auto result = validate_path(path);
    if (operation->cancelled.load() || core->shutting_down.load()) {
        result = {GR_STATUS_CANCELLED, GR_BOOK_FORMAT_UNKNOWN, "The operation was cancelled."};
    }
    try {
        callback(result.status, result.format, result.detail.c_str(), user_data);
    } catch (...) {
    }
}

} // namespace

extern "C" {

const char* gr_core_version(void) { return "0.1.0"; }

gr_status_t gr_core_create(gr_core_t** out_core) {
    if (out_core == nullptr) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    *out_core = nullptr;
    try {
        auto* core = new (std::nothrow) gr_core{std::make_shared<core_state>()};
        if (core == nullptr) {
            return GR_STATUS_INTERNAL_ERROR;
        }
        *out_core = core;
        return GR_STATUS_OK;
    } catch (...) {
        return GR_STATUS_INTERNAL_ERROR;
    }
}

void gr_core_destroy(gr_core_t* core) {
    if (core == nullptr) {
        return;
    }
    core->state->shutting_down.store(true);
    try {
        const std::lock_guard lock(core->state->operations_mutex);
        for (const auto& weak_operation : core->state->operations) {
            if (const auto operation = weak_operation.lock()) {
                operation->cancelled.store(true);
            }
        }
    } catch (...) {
    }
    delete core;
}

gr_status_t gr_core_validate_book_file(const gr_core_t* core, const char* path_utf8,
                                       gr_book_format_t* out_format) {
    if (out_format == nullptr) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    *out_format = GR_BOOK_FORMAT_UNKNOWN;
    const auto argument_status = validate_arguments(core, path_utf8);
    if (argument_status != GR_STATUS_OK) {
        return argument_status;
    }
    try {
        const auto result = validate_path(path_utf8);
        *out_format = result.format;
        return result.status;
    } catch (...) {
        return GR_STATUS_INTERNAL_ERROR;
    }
}

gr_status_t gr_core_open_book_file(const gr_core_t* core, const char* path_utf8,
                                   gr_book_document_t** out_document, char* detail_utf8,
                                   size_t detail_capacity, size_t* detail_required) {
    if (out_document == nullptr || core == nullptr || path_utf8 == nullptr) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    *out_document = nullptr;
    if (core->state->shutting_down.load())
        return GR_STATUS_CANCELLED;
    try {
        auto document = std::make_unique<gr_book_document>(
            gr_book_document{goodreader::reader::ReaderDocument::open(path_from_utf8(path_utf8))});
        copy_error_detail("", detail_utf8, detail_capacity, detail_required);
        *out_document = document.release();
        return GR_STATUS_OK;
    } catch (const goodreader::reader::DocumentError& error) {
        copy_error_detail(error.what(), detail_utf8, detail_capacity, detail_required);
        switch (error.code()) {
        case goodreader::reader::DocumentErrorCode::io:
            return GR_STATUS_IO_ERROR;
        case goodreader::reader::DocumentErrorCode::corrupt:
            return GR_STATUS_CORRUPT_BOOK;
        case goodreader::reader::DocumentErrorCode::unsupported:
            return GR_STATUS_UNSUPPORTED_FORMAT;
        case goodreader::reader::DocumentErrorCode::no_text_layer:
            return GR_STATUS_NO_TEXT_LAYER;
        }
    } catch (...) {
        copy_error_detail("The book could not be opened.", detail_utf8, detail_capacity,
                          detail_required);
        return GR_STATUS_INTERNAL_ERROR;
    }
    return GR_STATUS_INTERNAL_ERROR;
}

void gr_book_document_destroy(gr_book_document_t* document) { delete document; }

gr_status_t gr_book_document_copy_metadata(const gr_book_document_t* document, char* title_utf8,
                                           size_t title_capacity, size_t* title_required,
                                           char* author_utf8, size_t author_capacity,
                                           size_t* author_required) {
    if (document == nullptr || title_required == nullptr || author_required == nullptr) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    const auto title_status =
        copy_string(document->value.title, title_utf8, title_capacity, title_required);
    const auto author_status =
        copy_string(document->value.author, author_utf8, author_capacity, author_required);
    if (title_status == GR_STATUS_INVALID_ARGUMENT || author_status == GR_STATUS_INVALID_ARGUMENT) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    return title_status == GR_STATUS_OK && author_status == GR_STATUS_OK
               ? GR_STATUS_OK
               : GR_STATUS_BUFFER_TOO_SMALL;
}

gr_status_t gr_book_document_get_counts(const gr_book_document_t* document, size_t* chapter_count,
                                        size_t* word_count) {
    if (document == nullptr || chapter_count == nullptr || word_count == nullptr) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    *chapter_count = document->value.chapters.size();
    *word_count = document->value.word_count;
    return GR_STATUS_OK;
}

gr_status_t gr_book_document_copy_chapter_title(const gr_book_document_t* document,
                                                size_t chapter_index, char* title_utf8,
                                                size_t title_capacity, size_t* title_required,
                                                size_t* word_count) {
    if (document == nullptr || title_required == nullptr || word_count == nullptr ||
        chapter_index >= document->value.chapters.size()) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    const auto& chapter = document->value.chapters[chapter_index];
    *word_count = chapter.words.size();
    return copy_string(chapter.title, title_utf8, title_capacity, title_required);
}

gr_status_t gr_book_document_copy_word_range(const gr_book_document_t* document,
                                             size_t chapter_index, size_t first_word_index,
                                             size_t max_words, char* text_utf8,
                                             size_t text_capacity, size_t* required_bytes) {
    if (document == nullptr || required_bytes == nullptr ||
        chapter_index >= document->value.chapters.size() ||
        first_word_index > std::numeric_limits<std::uint32_t>::max() ||
        max_words > std::numeric_limits<std::uint32_t>::max() ||
        (text_utf8 == nullptr && text_capacity > 0)) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    try {
        const auto text = document->value.text_for_words(
            static_cast<std::uint32_t>(chapter_index), static_cast<std::uint32_t>(first_word_index),
            static_cast<std::uint32_t>(max_words));
        return copy_string(text, text_utf8, text_capacity, required_bytes);
    } catch (...) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
}

gr_status_t gr_book_document_position_for_global_word(const gr_book_document_t* document,
                                                      size_t global_word_index,
                                                      gr_text_position_t* out_position) {
    if (document == nullptr || out_position == nullptr ||
        global_word_index > std::numeric_limits<std::uint32_t>::max()) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    try {
        const auto position =
            document->value.position_for_global_word(static_cast<std::uint32_t>(global_word_index));
        out_position->chapter_index = position.chapter_index;
        out_position->word_index = position.word_index;
        return GR_STATUS_OK;
    } catch (...) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
}

gr_status_t gr_book_document_global_word_index(const gr_book_document_t* document,
                                               gr_text_position_t position,
                                               size_t* out_global_word_index) {
    if (document == nullptr || out_global_word_index == nullptr ||
        position.chapter_index > std::numeric_limits<std::uint32_t>::max() ||
        position.word_index > std::numeric_limits<std::uint32_t>::max()) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    try {
        *out_global_word_index =
            document->value.global_word_index({static_cast<std::uint32_t>(position.chapter_index),
                                               static_cast<std::uint32_t>(position.word_index)});
        return GR_STATUS_OK;
    } catch (...) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
}

gr_status_t gr_alignment_index_create(const gr_alignment_anchor_t* anchors, size_t anchor_count,
                                      gr_alignment_index_t** out_index) {
    if (out_index == nullptr || anchors == nullptr || anchor_count == 0 ||
        anchor_count > std::numeric_limits<std::uint32_t>::max()) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    *out_index = nullptr;
    try {
        std::vector<goodreader::domain::AlignmentAnchor> values;
        values.reserve(anchor_count);
        for (size_t index = 0; index < anchor_count; ++index) {
            if (anchors[index].global_word_index > std::numeric_limits<std::int32_t>::max()) {
                return GR_STATUS_INVALID_ARGUMENT;
            }
            values.emplace_back(goodreader::domain::WordOffset(
                                    static_cast<std::int32_t>(anchors[index].global_word_index)),
                                anchors[index].audio_time_millis, anchors[index].confidence);
        }
        auto index = std::make_unique<gr_alignment_index>(
            gr_alignment_index{goodreader::domain::AlignmentIndex(std::move(values))});
        *out_index = index.release();
        return GR_STATUS_OK;
    } catch (const std::invalid_argument&) {
        return GR_STATUS_INVALID_ARGUMENT;
    } catch (...) {
        return GR_STATUS_INTERNAL_ERROR;
    }
}

void gr_alignment_index_destroy(gr_alignment_index_t* index) { delete index; }

gr_status_t gr_book_document_position_at_audio_time(const gr_book_document_t* document,
                                                    const gr_alignment_index_t* index,
                                                    int64_t audio_time_millis,
                                                    gr_text_position_t* out_position) {
    if (document == nullptr || index == nullptr || out_position == nullptr ||
        audio_time_millis < 0) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    try {
        const auto word = index->value.word_at_audio_time(audio_time_millis);
        if (!word.has_value())
            return GR_STATUS_NOT_FOUND;
        if (static_cast<std::size_t>(word->value) >= document->value.word_count) {
            return GR_STATUS_INVALID_ARGUMENT;
        }
        const auto position =
            document->value.position_for_global_word(static_cast<std::uint32_t>(word->value));
        out_position->chapter_index = position.chapter_index;
        out_position->word_index = position.word_index;
        return GR_STATUS_OK;
    } catch (...) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
}

gr_status_t gr_book_document_audio_time_for_position(const gr_book_document_t* document,
                                                     const gr_alignment_index_t* index,
                                                     gr_text_position_t position,
                                                     int64_t* out_audio_time_millis) {
    if (document == nullptr || index == nullptr || out_audio_time_millis == nullptr ||
        position.chapter_index > std::numeric_limits<std::uint32_t>::max() ||
        position.word_index > std::numeric_limits<std::uint32_t>::max()) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    try {
        const auto global_word =
            document->value.global_word_index({static_cast<std::uint32_t>(position.chapter_index),
                                               static_cast<std::uint32_t>(position.word_index)});
        const auto audio_time = index->value.audio_time_at_word(
            goodreader::domain::WordOffset(static_cast<std::int32_t>(global_word)));
        if (!audio_time.has_value())
            return GR_STATUS_NOT_FOUND;
        *out_audio_time_millis = *audio_time;
        return GR_STATUS_OK;
    } catch (...) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
}

gr_status_t gr_core_validate_book_file_async(gr_core_t* core, const char* path_utf8,
                                             gr_validation_callback_t callback, void* user_data,
                                             gr_operation_t** out_operation) {
    if (out_operation == nullptr) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    *out_operation = nullptr;
    if (core == nullptr || path_utf8 == nullptr || callback == nullptr) {
        return GR_STATUS_INVALID_ARGUMENT;
    }

    const auto argument_status = validate_arguments(core, path_utf8);
    if (argument_status != GR_STATUS_OK) {
        return argument_status;
    }

    try {
        const std::string path(path_utf8);
        auto operation_state_ptr = std::make_shared<operation_state>();
        std::unique_ptr<gr_operation> operation(new (std::nothrow)
                                                    gr_operation{operation_state_ptr});
        if (!operation) {
            return GR_STATUS_INTERNAL_ERROR;
        }

        {
            const std::lock_guard lock(core->state->operations_mutex);
            if (core->state->shutting_down.load()) {
                return GR_STATUS_CANCELLED;
            }
            std::erase_if(core->state->operations,
                          [](const auto& weak_operation) { return weak_operation.expired(); });
            core->state->operations.emplace_back(operation_state_ptr);
        }

        const auto core_state_ptr = core->state;
        std::thread worker([operation_state_ptr, core_state_ptr, path, callback, user_data] {
            try {
                invoke_callback(operation_state_ptr, core_state_ptr, path, callback, user_data);
            } catch (...) {
                try {
                    callback(GR_STATUS_INTERNAL_ERROR, GR_BOOK_FORMAT_UNKNOWN,
                             "The asynchronous operation failed.", user_data);
                } catch (...) {
                }
            }
        });
        try {
            worker.detach();
        } catch (...) {
            worker.join();
            return GR_STATUS_INTERNAL_ERROR;
        }

        *out_operation = operation.release();
        return GR_STATUS_OK;
    } catch (...) {
        return GR_STATUS_INTERNAL_ERROR;
    }
}

gr_status_t gr_operation_cancel(gr_operation_t* operation) {
    if (operation == nullptr) {
        return GR_STATUS_INVALID_ARGUMENT;
    }
    operation->state->cancelled.store(true);
    return GR_STATUS_OK;
}

void gr_operation_release(gr_operation_t* operation) { delete operation; }

} // extern "C"
