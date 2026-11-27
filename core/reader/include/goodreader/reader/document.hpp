#pragma once

#include "goodreader/domain/types.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace goodreader::reader {

enum class DocumentErrorCode { io, corrupt, unsupported, no_text_layer };

class DocumentError final : public std::runtime_error {
  public:
    DocumentError(DocumentErrorCode code, std::string message);
    DocumentErrorCode code() const noexcept;

  private:
    DocumentErrorCode code_;
};

/** Stable location within a chapter; it does not depend on visual pagination. */
struct TextPosition {
    std::uint32_t chapter_index;
    std::uint32_t word_index;
    bool operator==(const TextPosition&) const = default;
};

/** A token and its offset in Unicode characters and UTF-8 bytes within the chapter text. */
struct ReaderWord {
    std::uint32_t index;
    std::uint32_t character_offset;
    std::size_t byte_start;
    std::size_t byte_end;
    std::string text;
};

struct ReaderParagraph {
    std::string text;
    std::uint32_t first_word_index;
    std::uint32_t word_count;
};

struct ReaderChapter {
    std::string title;
    std::string text;
    std::vector<ReaderParagraph> paragraphs;
    std::vector<ReaderWord> words;
};

/** Parsed, reflowable book content shared by all platform reader views. */
struct ReaderDocument {
    std::string title;
    std::string author;
    domain::BookFormat format;
    std::vector<ReaderChapter> chapters;
    std::uint32_t word_count = 0;

    static ReaderDocument open(const std::filesystem::path& path);
    std::string text_for_words(std::uint32_t chapter_index, std::uint32_t first_word_index,
                               std::uint32_t word_count) const;
    TextPosition position_for_global_word(std::uint32_t global_word_index) const;
    std::uint32_t global_word_index(TextPosition position) const;
};

} // namespace goodreader::reader
