#include "goodreader/alignment/text_normalizer.hpp"

#include <utf8proc.h>

#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>

namespace goodreader::alignment {
namespace {

[[nodiscard]] bool is_letter_or_number(utf8proc_category_t category) {
    switch (category) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
    case UTF8PROC_CATEGORY_ND:
    case UTF8PROC_CATEGORY_NL:
    case UTF8PROC_CATEGORY_NO:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_decimal_digit(utf8proc_int32_t character) {
    return utf8proc_category(character) == UTF8PROC_CATEGORY_ND;
}

[[nodiscard]] std::string encode_character(utf8proc_int32_t character) {
    utf8proc_uint8_t encoded[4]{};
    const auto length = utf8proc_encode_char(character, encoded);
    if (length <= 0) {
        throw std::runtime_error("Unicode token could not be encoded.");
    }
    return std::string(reinterpret_cast<const char*>(encoded), static_cast<std::size_t>(length));
}

} // namespace

std::vector<std::string> TextNormalizer::tokenize(const std::string& text) {
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<utf8proc_ssize_t>::max())) {
        throw std::length_error("Text is too large to normalize.");
    }

    utf8proc_uint8_t* normalized_raw = nullptr;
    const auto normalized_length =
        utf8proc_map(reinterpret_cast<const utf8proc_uint8_t*>(text.data()),
                     static_cast<utf8proc_ssize_t>(text.size()), &normalized_raw,
                     static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_CASEFOLD |
                                                    UTF8PROC_DECOMPOSE | UTF8PROC_STRIPMARK));
    if (normalized_length < 0) {
        throw std::invalid_argument("Text contains invalid UTF-8.");
    }
    const std::unique_ptr<utf8proc_uint8_t, decltype(&std::free)> normalized(normalized_raw,
                                                                             &std::free);

    std::vector<utf8proc_int32_t> characters;
    auto offset = static_cast<utf8proc_ssize_t>(0);
    while (offset < normalized_length) {
        utf8proc_int32_t character = 0;
        const auto consumed =
            utf8proc_iterate(normalized.get() + offset, normalized_length - offset, &character);
        if (consumed <= 0) {
            throw std::invalid_argument("Text contains invalid UTF-8.");
        }
        characters.push_back(character);
        offset += consumed;
    }

    std::vector<std::string> tokens;
    std::string token;
    for (std::size_t index = 0; index < characters.size(); ++index) {
        const auto character = characters[index];
        if (is_letter_or_number(utf8proc_category(character))) {
            token += encode_character(character);
            continue;
        }
        const bool numeric_separator =
            (character == ',' || character == '.') && index > 0 && index + 1 < characters.size() &&
            is_decimal_digit(characters[index - 1]) && is_decimal_digit(characters[index + 1]);
        if (numeric_separator) {
            continue;
        }
        if (!token.empty()) {
            tokens.push_back(std::move(token));
            token.clear();
        }
    }
    if (!token.empty()) {
        tokens.push_back(std::move(token));
    }
    return tokens;
}

} // namespace goodreader::alignment
