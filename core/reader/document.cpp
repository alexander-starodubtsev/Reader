#include "goodreader/reader/document.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

namespace goodreader::reader {
namespace {

constexpr std::size_t max_file_size = 128U * 1024U * 1024U;
constexpr std::size_t max_member_size = 64U * 1024U * 1024U;

struct Utf8Character {
    std::uint32_t value;
    std::size_t byte_length;
};

[[nodiscard]] Utf8Character decode_utf8(std::string_view text, std::size_t offset) {
    if (offset >= text.size()) {
        throw DocumentError(DocumentErrorCode::corrupt, "The book contains invalid UTF-8.");
    }
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first < 0x80U)
        return {first, 1};
    std::size_t length = 0;
    std::uint32_t value = 0;
    std::uint32_t minimum = 0;
    if ((first & 0xE0U) == 0xC0U) {
        length = 2;
        value = first & 0x1FU;
        minimum = 0x80U;
    } else if ((first & 0xF0U) == 0xE0U) {
        length = 3;
        value = first & 0x0FU;
        minimum = 0x800U;
    } else if ((first & 0xF8U) == 0xF0U) {
        length = 4;
        value = first & 0x07U;
        minimum = 0x10000U;
    } else {
        throw DocumentError(DocumentErrorCode::corrupt, "The book contains invalid UTF-8.");
    }
    if (offset + length > text.size()) {
        throw DocumentError(DocumentErrorCode::corrupt, "The book contains invalid UTF-8.");
    }
    for (std::size_t index = 1; index < length; ++index) {
        const auto next = static_cast<unsigned char>(text[offset + index]);
        if ((next & 0xC0U) != 0x80U) {
            throw DocumentError(DocumentErrorCode::corrupt, "The book contains invalid UTF-8.");
        }
        value = (value << 6U) | (next & 0x3FU);
    }
    if (value < minimum || value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU)) {
        throw DocumentError(DocumentErrorCode::corrupt, "The book contains invalid UTF-8.");
    }
    return {value, length};
}

[[nodiscard]] std::string encode_utf8(std::uint32_t value) {
    if (value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU)) {
        throw DocumentError(DocumentErrorCode::corrupt, "The book contains invalid XML.");
    }
    std::string encoded;
    if (value < 0x80U) {
        encoded.push_back(static_cast<char>(value));
    } else if (value < 0x800U) {
        encoded.push_back(static_cast<char>(0xC0U | (value >> 6U)));
        encoded.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    } else if (value < 0x10000U) {
        encoded.push_back(static_cast<char>(0xE0U | (value >> 12U)));
        encoded.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
        encoded.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    } else {
        encoded.push_back(static_cast<char>(0xF0U | (value >> 18U)));
        encoded.push_back(static_cast<char>(0x80U | ((value >> 12U) & 0x3FU)));
        encoded.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
        encoded.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    }
    return encoded;
}

[[nodiscard]] std::uint16_t read_u16(std::string_view bytes, std::size_t offset) {
    if (offset + 2 > bytes.size()) {
        throw DocumentError(DocumentErrorCode::corrupt, "The book archive is truncated.");
    }
    return static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[offset])) |
           static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[offset + 1])
                                      << static_cast<unsigned int>(8));
}

[[nodiscard]] std::uint32_t read_u32(std::string_view bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) {
        throw DocumentError(DocumentErrorCode::corrupt, "The book archive is truncated.");
    }
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset])) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8U) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 2])) << 16U) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 3])) << 24U);
}

[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > max_file_size) {
        throw DocumentError(DocumentErrorCode::io, "The selected book cannot be read.");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw DocumentError(DocumentErrorCode::io, "The selected book cannot be opened.");
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    input.read(result.data(), static_cast<std::streamsize>(result.size()));
    if (!input && !input.eof()) {
        throw DocumentError(DocumentErrorCode::io, "The selected book could not be read.");
    }
    return result;
}

[[nodiscard]] std::string inflate_raw(std::string_view compressed, std::size_t expected_size) {
    if (expected_size > max_member_size) {
        throw DocumentError(DocumentErrorCode::corrupt, "A book chapter is too large to open.");
    }
    std::string output(expected_size, '\0');
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        throw DocumentError(DocumentErrorCode::corrupt,
                            "A book chapter is compressed incorrectly.");
    }
    const auto status = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (status != Z_STREAM_END || stream.total_out != expected_size) {
        throw DocumentError(DocumentErrorCode::corrupt,
                            "A book chapter is compressed incorrectly.");
    }
    return output;
}

class ZipArchive final {
  public:
    explicit ZipArchive(std::string bytes) : bytes_(std::move(bytes)) {}

    [[nodiscard]] std::string member(std::string_view requested_name) const {
        const auto end_record = find_end_record();
        const auto entry_count = read_u16(bytes_, end_record + 10);
        const auto central_offset = read_u32(bytes_, end_record + 16);
        std::size_t entry = central_offset;
        for (std::uint16_t index = 0; index < entry_count; ++index) {
            if (read_u32(bytes_, entry) != 0x02014b50U) {
                throw DocumentError(DocumentErrorCode::corrupt, "The EPUB archive is invalid.");
            }
            const auto compressed_size = read_u32(bytes_, entry + 20);
            const auto expanded_size = read_u32(bytes_, entry + 24);
            const auto name_length = read_u16(bytes_, entry + 28);
            const auto extra_length = read_u16(bytes_, entry + 30);
            const auto comment_length = read_u16(bytes_, entry + 32);
            const auto local_offset = read_u32(bytes_, entry + 42);
            const auto name_start = entry + 46;
            if (name_start + name_length > bytes_.size()) {
                throw DocumentError(DocumentErrorCode::corrupt, "The EPUB archive is invalid.");
            }
            const std::string_view name(bytes_.data() + name_start, name_length);
            if (name == requested_name) {
                if (read_u32(bytes_, local_offset) != 0x04034b50U) {
                    throw DocumentError(DocumentErrorCode::corrupt, "The EPUB archive is invalid.");
                }
                const auto local_name_length = read_u16(bytes_, local_offset + 26);
                const auto local_extra_length = read_u16(bytes_, local_offset + 28);
                const auto data_start = local_offset + 30 + local_name_length + local_extra_length;
                if (data_start + compressed_size > bytes_.size()) {
                    throw DocumentError(DocumentErrorCode::corrupt, "The EPUB archive is invalid.");
                }
                const std::string_view compressed(bytes_.data() + data_start, compressed_size);
                const auto method = read_u16(bytes_, entry + 10);
                if (method == 0) {
                    return std::string(compressed);
                }
                if (method == 8) {
                    return inflate_raw(compressed, expanded_size);
                }
                throw DocumentError(DocumentErrorCode::unsupported,
                                    "This EPUB uses an unsupported compression method.");
            }
            entry = name_start + name_length + extra_length + comment_length;
        }
        throw DocumentError(DocumentErrorCode::corrupt, "An EPUB content file is missing.");
    }

  private:
    [[nodiscard]] std::size_t find_end_record() const {
        if (bytes_.size() < 22) {
            throw DocumentError(DocumentErrorCode::corrupt, "The EPUB archive is truncated.");
        }
        const auto lower_bound = bytes_.size() > 65'557 ? bytes_.size() - 65'557 : 0;
        for (auto offset = bytes_.size() - 22;; --offset) {
            if (read_u32(bytes_, offset) == 0x06054b50U) {
                return offset;
            }
            if (offset == lower_bound) {
                break;
            }
        }
        throw DocumentError(DocumentErrorCode::corrupt, "The EPUB archive is invalid.");
    }

    std::string bytes_;
};

[[nodiscard]] std::string local_name(std::string_view name) {
    const auto separator = name.rfind(':');
    return std::string(separator == std::string_view::npos ? name : name.substr(separator + 1));
}

struct Tag {
    std::string name;
    std::string attributes;
    bool closing = false;
    bool self_closing = false;
    std::size_t start = 0;
    std::size_t end = 0;
};

[[nodiscard]] std::optional<Tag> next_tag(std::string_view source, std::size_t& offset) {
    const auto open = source.find('<', offset);
    if (open == std::string_view::npos) {
        return std::nullopt;
    }
    const auto close = source.find('>', open + 1);
    if (close == std::string_view::npos) {
        return std::nullopt;
    }
    offset = close + 1;
    auto content = source.substr(open + 1, close - open - 1);
    if (content.empty() || content.front() == '!' || content.front() == '?') {
        return next_tag(source, offset);
    }
    Tag tag;
    tag.start = open;
    if (content.front() == '/') {
        tag.closing = true;
        content.remove_prefix(1);
    }
    const auto name_end = content.find_first_of(" \t\r\n/");
    tag.name = local_name(content.substr(0, name_end));
    if (name_end != std::string_view::npos) {
        tag.attributes = std::string(content.substr(name_end));
        tag.self_closing = tag.attributes.find_last_not_of(" \t\r\n") != std::string::npos &&
                           tag.attributes[tag.attributes.find_last_not_of(" \t\r\n")] == '/';
    }
    tag.end = close + 1;
    return tag;
}

[[nodiscard]] std::string decode_entities(std::string_view value) {
    std::string decoded;
    for (std::size_t index = 0; index < value.size();) {
        if (value[index] != '&') {
            decoded.push_back(value[index++]);
            continue;
        }
        const auto end = value.find(';', index + 1);
        if (end == std::string_view::npos || end - index > 12) {
            decoded.push_back(value[index++]);
            continue;
        }
        const auto entity = value.substr(index + 1, end - index - 1);
        if (entity == "amp")
            decoded.push_back('&');
        else if (entity == "lt")
            decoded.push_back('<');
        else if (entity == "gt")
            decoded.push_back('>');
        else if (entity == "quot")
            decoded.push_back('"');
        else if (entity == "apos")
            decoded.push_back('\'');
        else if (entity.starts_with("#")) {
            try {
                const auto base =
                    entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X') ? 16 : 10;
                const auto start = base == 16 ? 2U : 1U;
                const auto point = static_cast<std::uint32_t>(
                    std::stoul(std::string(entity.substr(start)), nullptr, base));
                decoded += encode_utf8(point);
            } catch (const std::exception&) {
                throw DocumentError(DocumentErrorCode::corrupt, "The book contains invalid XML.");
            }
        } else {
            decoded.append(value.substr(index, end - index + 1));
        }
        index = end + 1;
    }
    return decoded;
}

[[nodiscard]] std::string normalize_whitespace(std::string value) {
    std::string result;
    bool pending_space = false;
    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (std::isspace(byte) != 0) {
            pending_space = !result.empty();
        } else {
            if (pending_space)
                result.push_back(' ');
            result.push_back(character);
            pending_space = false;
        }
    }
    return result;
}

[[nodiscard]] std::vector<std::string> element_contents(std::string_view source,
                                                        std::string_view requested_name) {
    std::vector<std::string> result;
    std::size_t offset = 0;
    while (const auto tag = next_tag(source, offset)) {
        if (tag->closing || tag->name != requested_name || tag->self_closing)
            continue;
        const auto content_start = offset;
        auto depth = 1U;
        while (const auto nested = next_tag(source, offset)) {
            if (nested->name != requested_name)
                continue;
            if (nested->closing) {
                --depth;
                if (depth == 0) {
                    result.emplace_back(
                        source.substr(content_start, nested->start - content_start));
                    break;
                }
            } else if (!nested->self_closing) {
                ++depth;
            }
        }
    }
    return result;
}

[[nodiscard]] std::string attribute_value(std::string_view attributes,
                                          std::string_view requested_name) {
    std::size_t offset = 0;
    while (offset < attributes.size()) {
        while (offset < attributes.size() &&
               std::isspace(static_cast<unsigned char>(attributes[offset])) != 0)
            ++offset;
        const auto start = offset;
        while (offset < attributes.size() &&
               (std::isalnum(static_cast<unsigned char>(attributes[offset])) != 0 ||
                attributes[offset] == '-' || attributes[offset] == ':' ||
                attributes[offset] == '_'))
            ++offset;
        const auto name = attributes.substr(start, offset - start);
        while (offset < attributes.size() &&
               std::isspace(static_cast<unsigned char>(attributes[offset])) != 0)
            ++offset;
        if (offset >= attributes.size() || attributes[offset] != '=') {
            while (offset < attributes.size() && attributes[offset] != ' ')
                ++offset;
            continue;
        }
        ++offset;
        while (offset < attributes.size() &&
               std::isspace(static_cast<unsigned char>(attributes[offset])) != 0)
            ++offset;
        if (offset >= attributes.size() ||
            (attributes[offset] != '"' && attributes[offset] != '\'')) {
            continue;
        }
        const auto quote = attributes[offset++];
        const auto value_start = offset;
        const auto end = attributes.find(quote, offset);
        if (end == std::string_view::npos)
            return {};
        if (name == requested_name)
            return decode_entities(attributes.substr(value_start, end - value_start));
        offset = end + 1;
    }
    return {};
}

[[nodiscard]] std::string text_content(std::string_view markup) {
    std::string text;
    bool in_tag = false;
    for (const auto character : markup) {
        if (character == '<') {
            in_tag = true;
            continue;
        }
        if (character == '>') {
            in_tag = false;
            continue;
        }
        if (!in_tag)
            text.push_back(character);
    }
    return normalize_whitespace(decode_entities(text));
}

[[nodiscard]] std::string first_text(std::string_view source, std::string_view name) {
    const auto elements = element_contents(source, name);
    return elements.empty() ? std::string{} : text_content(elements.front());
}

[[nodiscard]] std::vector<std::string> paragraph_texts(std::string_view source) {
    std::vector<std::string> paragraphs;
    for (const auto& markup : element_contents(source, "p")) {
        auto text = text_content(markup);
        if (!text.empty())
            paragraphs.push_back(std::move(text));
    }
    return paragraphs;
}

[[nodiscard]] std::vector<std::string> top_level_sections(std::string_view source) {
    std::vector<std::string> sections;
    std::size_t offset = 0;
    std::size_t section_start = 0;
    unsigned int depth = 0;
    while (const auto tag = next_tag(source, offset)) {
        if (tag->name != "section")
            continue;
        if (tag->closing) {
            if (depth > 0)
                --depth;
            if (depth == 0 && section_start <= offset) {
                sections.emplace_back(source.substr(section_start, tag->start - section_start));
            }
        } else if (!tag->self_closing) {
            if (depth == 0)
                section_start = offset;
            ++depth;
        }
    }
    return sections;
}

[[nodiscard]] bool is_word_character(std::uint32_t point) {
    if (point < 0x80U) {
        return std::isalnum(static_cast<unsigned char>(point)) != 0;
    }
    if ((point >= 0x2000U && point <= 0x206FU) || (point >= 0x3000U && point <= 0x303FU) ||
        (point >= 0x1F000U && point <= 0x1FAFFU) || point == 0x00A0U || point == 0x00ADU) {
        return false;
    }
    return true;
}

void add_chapter(ReaderDocument& document, std::string title,
                 const std::vector<std::string>& paragraphs) {
    ReaderChapter chapter;
    chapter.title = std::move(title);
    if (chapter.title.empty())
        chapter.title = "Chapter " + std::to_string(document.chapters.size() + 1);
    for (const auto& paragraph : paragraphs) {
        if (paragraph.empty())
            continue;
        if (!chapter.text.empty())
            chapter.text += "\n\n";
        const auto paragraph_start = chapter.text.size();
        chapter.text += paragraph;
        const auto first_word = static_cast<std::uint32_t>(chapter.words.size());
        std::uint32_t character_offset = 0;
        for (std::size_t index = 0; index < paragraph_start; ++index) {
            if ((static_cast<unsigned char>(chapter.text[index]) & 0xC0U) != 0x80U) {
                ++character_offset;
            }
        }
        std::size_t byte_offset = paragraph_start;
        std::optional<std::size_t> word_start;
        std::uint32_t word_char_start = 0;
        std::uint32_t char_offset = character_offset;
        while (byte_offset < chapter.text.size()) {
            const auto character = decode_utf8(chapter.text, byte_offset);
            if (is_word_character(character.value)) {
                if (!word_start.has_value()) {
                    word_start = byte_offset;
                    word_char_start = char_offset;
                }
            } else if (word_start.has_value()) {
                chapter.words.push_back(
                    {static_cast<std::uint32_t>(chapter.words.size()), word_char_start, *word_start,
                     byte_offset, chapter.text.substr(*word_start, byte_offset - *word_start)});
                word_start.reset();
            }
            byte_offset += character.byte_length;
            ++char_offset;
        }
        if (word_start.has_value()) {
            chapter.words.push_back({static_cast<std::uint32_t>(chapter.words.size()),
                                     word_char_start, *word_start, byte_offset,
                                     chapter.text.substr(*word_start, byte_offset - *word_start)});
        }
        chapter.paragraphs.push_back(
            {paragraph, first_word, static_cast<std::uint32_t>(chapter.words.size()) - first_word});
    }
    if (!chapter.words.empty()) {
        document.word_count += static_cast<std::uint32_t>(chapter.words.size());
        document.chapters.push_back(std::move(chapter));
    }
}

[[nodiscard]] std::string find_pdf_text(std::string_view pdf) {
    std::string extracted;
    std::size_t stream_position = 0;
    while ((stream_position = pdf.find("stream", stream_position)) != std::string_view::npos) {
        auto data_start = stream_position + 6;
        if (data_start < pdf.size() && pdf[data_start] == '\r')
            ++data_start;
        if (data_start < pdf.size() && pdf[data_start] == '\n')
            ++data_start;
        const auto data_end = pdf.find("endstream", data_start);
        if (data_end == std::string_view::npos)
            break;
        const auto dictionary_start = pdf.rfind("<<", stream_position);
        const auto dictionary =
            dictionary_start == std::string_view::npos
                ? std::string_view{}
                : pdf.substr(dictionary_start, stream_position - dictionary_start);
        const auto compressed = pdf.substr(data_start, data_end - data_start);
        std::string content;
        if (dictionary.find("/FlateDecode") != std::string_view::npos) {
            z_stream stream{};
            stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
            stream.avail_in = static_cast<uInt>(compressed.size());
            std::array<char, 4096> chunk{};
            if (inflateInit(&stream) == Z_OK) {
                int status = Z_OK;
                while (status == Z_OK && content.size() < max_member_size) {
                    stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
                    stream.avail_out = static_cast<uInt>(chunk.size());
                    status = inflate(&stream, Z_NO_FLUSH);
                    content.append(chunk.data(), chunk.size() - stream.avail_out);
                }
                inflateEnd(&stream);
                if (status != Z_STREAM_END)
                    content.clear();
            }
        } else {
            content.assign(compressed);
        }

        auto text_start = std::size_t{0};
        while ((text_start = content.find("BT", text_start)) != std::string::npos) {
            const auto text_end = content.find("ET", text_start + 2);
            if (text_end == std::string::npos)
                break;
            for (auto index = text_start + 2; index < text_end; ++index) {
                if (content[index] != '(')
                    continue;
                std::string literal;
                unsigned int nesting = 1;
                ++index;
                while (index < text_end && nesting > 0) {
                    const auto character = content[index++];
                    if (character == '\\' && index < text_end) {
                        const auto escaped = content[index++];
                        if (escaped == 'n')
                            literal.push_back('\n');
                        else if (escaped == 'r')
                            literal.push_back('\r');
                        else if (escaped == 't')
                            literal.push_back('\t');
                        else
                            literal.push_back(escaped);
                    } else if (character == '(') {
                        ++nesting;
                        literal.push_back(character);
                    } else if (character == ')') {
                        --nesting;
                        if (nesting > 0)
                            literal.push_back(character);
                    } else {
                        literal.push_back(character);
                    }
                }
                if (!literal.empty()) {
                    if (!extracted.empty())
                        extracted.push_back(' ');
                    extracted += literal;
                }
            }
            text_start = text_end + 2;
        }
        stream_position = data_end + 9;
    }
    return normalize_whitespace(std::move(extracted));
}

[[nodiscard]] std::filesystem::path path_from_utf8(const std::string& value) {
    std::u8string encoded;
    encoded.reserve(value.size());
    for (const auto character : value) {
        encoded.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    }
    return std::filesystem::path(encoded);
}

[[nodiscard]] std::string normalize_archive_path(std::string base, std::string href) {
    std::filesystem::path path = path_from_utf8(base).parent_path() / path_from_utf8(href);
    path = path.lexically_normal();
    const auto normalized = path.generic_string();
    if (normalized.empty() || normalized == ".." || normalized.starts_with("../") ||
        normalized.starts_with('/')) {
        throw DocumentError(DocumentErrorCode::corrupt, "The EPUB contains an invalid path.");
    }
    return normalized;
}

void parse_epub(ReaderDocument& document, const std::string& archive_bytes) {
    const ZipArchive archive(archive_bytes);
    const auto container = archive.member("META-INF/container.xml");
    std::string package_path;
    std::size_t container_offset = 0;
    while (const auto tag = next_tag(container, container_offset)) {
        if (tag->name == "rootfile" && !tag->closing) {
            package_path = attribute_value(tag->attributes, "full-path");
            break;
        }
    }
    if (package_path.empty()) {
        throw DocumentError(DocumentErrorCode::corrupt, "The EPUB package file is missing.");
    }
    const auto package = archive.member(package_path);
    document.title = first_text(package, "title");
    const auto creator = first_text(package, "creator");
    document.author = creator;

    std::map<std::string, std::string> manifest;
    std::size_t offset = 0;
    while (const auto tag = next_tag(package, offset)) {
        if (tag->name != "item" || tag->closing)
            continue;
        const auto id = attribute_value(tag->attributes, "id");
        const auto href = attribute_value(tag->attributes, "href");
        const auto type = attribute_value(tag->attributes, "media-type");
        if (!id.empty() && !href.empty() &&
            (type == "application/xhtml+xml" || type == "text/html")) {
            manifest[id] = normalize_archive_path(package_path, href);
        }
    }

    const auto spine_chunks = element_contents(package, "spine");
    if (spine_chunks.empty()) {
        throw DocumentError(DocumentErrorCode::corrupt, "The EPUB reading order is missing.");
    }
    offset = 0;
    while (const auto tag = next_tag(spine_chunks.front(), offset)) {
        if (tag->name != "itemref" || tag->closing)
            continue;
        const auto idref = attribute_value(tag->attributes, "idref");
        const auto member_path = manifest.find(idref);
        if (member_path == manifest.end())
            continue;
        const auto content = archive.member(member_path->second);
        auto sections = top_level_sections(content);
        if (sections.empty())
            sections.push_back(content);
        for (const auto& section : sections) {
            auto paragraphs = paragraph_texts(section);
            if (paragraphs.empty())
                continue;
            auto title = first_text(section, "h1");
            if (title.empty())
                title = first_text(section, "h2");
            if (title.empty())
                title = first_text(section, "title");
            if (title.empty() && document.chapters.empty())
                title = document.title;
            add_chapter(document, std::move(title), paragraphs);
        }
    }
}

void parse_fb2(ReaderDocument& document, const std::string& source) {
    document.title = first_text(source, "book-title");
    const auto authors = element_contents(source, "author");
    if (!authors.empty()) {
        const auto& author = authors.front();
        document.author = first_text(author, "first-name");
        const auto middle = first_text(author, "middle-name");
        const auto last = first_text(author, "last-name");
        for (const auto& part : {middle, last}) {
            if (!part.empty()) {
                if (!document.author.empty())
                    document.author.push_back(' ');
                document.author += part;
            }
        }
    }
    const auto bodies = element_contents(source, "body");
    const auto& body = bodies.empty() ? source : bodies.front();
    auto sections = top_level_sections(body);
    if (sections.empty())
        sections.push_back(body);
    for (const auto& section : sections) {
        auto paragraphs = paragraph_texts(section);
        if (paragraphs.empty())
            continue;
        auto title = first_text(section, "title");
        if (title.empty() && document.chapters.empty())
            title = document.title;
        add_chapter(document, std::move(title), paragraphs);
    }
}

[[nodiscard]] std::string file_name_without_extension(const std::filesystem::path& path) {
    auto name = path.stem().string();
    return name.empty() ? "Untitled book" : name;
}

} // namespace

DocumentError::DocumentError(DocumentErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

DocumentErrorCode DocumentError::code() const noexcept { return code_; }

ReaderDocument ReaderDocument::open(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    const auto source = read_file(path);
    ReaderDocument document{"", "", domain::BookFormat::fb2, {}, 0};
    if (extension == ".epub") {
        document.format = domain::BookFormat::epub;
        parse_epub(document, source);
    } else if (extension == ".fb2") {
        document.format = domain::BookFormat::fb2;
        parse_fb2(document, source);
    } else if (extension == ".pdf") {
        document.format = domain::BookFormat::pdf;
        auto text = find_pdf_text(source);
        if (text.empty()) {
            throw DocumentError(
                DocumentErrorCode::no_text_layer,
                "This PDF has no selectable text. Scanned books cannot be read yet.");
        }
        add_chapter(document, file_name_without_extension(path), {text});
    } else {
        throw DocumentError(DocumentErrorCode::unsupported,
                            "Choose an EPUB, FB2, or PDF book file.");
    }
    if (document.title.empty())
        document.title = file_name_without_extension(path);
    if (document.chapters.empty()) {
        throw DocumentError(DocumentErrorCode::corrupt, "The book contains no readable text.");
    }
    return document;
}

std::string ReaderDocument::text_for_words(std::uint32_t chapter_index,
                                           std::uint32_t first_word_index,
                                           std::uint32_t requested_word_count) const {
    if (chapter_index >= chapters.size()) {
        throw std::out_of_range("Chapter index is outside the book.");
    }
    const auto& chapter = chapters[chapter_index];
    if (first_word_index >= chapter.words.size() || requested_word_count == 0)
        return {};
    const auto last_index =
        std::min<std::size_t>(chapter.words.size(),
                              static_cast<std::size_t>(first_word_index) + requested_word_count) -
        1;
    const auto start = chapter.words[first_word_index].byte_start;
    const auto end = chapter.words[last_index].byte_end;
    return chapter.text.substr(start, end - start);
}

TextPosition ReaderDocument::position_for_global_word(std::uint32_t global_word_index) const {
    if (global_word_index >= word_count) {
        throw std::out_of_range("Word index is outside the book.");
    }
    auto remaining = global_word_index;
    for (std::size_t chapter = 0; chapter < chapters.size(); ++chapter) {
        if (remaining < chapters[chapter].words.size()) {
            return {static_cast<std::uint32_t>(chapter), remaining};
        }
        remaining -= static_cast<std::uint32_t>(chapters[chapter].words.size());
    }
    throw std::out_of_range("Word index is outside the book.");
}

std::uint32_t ReaderDocument::global_word_index(TextPosition position) const {
    if (position.chapter_index >= chapters.size() ||
        position.word_index >= chapters[position.chapter_index].words.size()) {
        throw std::out_of_range("Text position is outside the book.");
    }
    std::uint32_t global_index = position.word_index;
    for (std::uint32_t chapter = 0; chapter < position.chapter_index; ++chapter) {
        global_index += static_cast<std::uint32_t>(chapters[chapter].words.size());
    }
    return global_index;
}

} // namespace goodreader::reader
