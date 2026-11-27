#include "goodreader/reader/document.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

struct ScopedFile {
    std::filesystem::path path;
    ~ScopedFile() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};

std::filesystem::path sample_file(const std::string& name) {
    return std::filesystem::path(GOODREADER_TEST_DATA_DIR) / name;
}

TEST(reader_document_tests, parsesEpubSpineSectionsAndStableWordPositions) {
    const auto document = goodreader::reader::ReaderDocument::open(sample_file("sample.epub"));

    ASSERT_EQ(document.format, goodreader::domain::BookFormat::epub);
    EXPECT_EQ(document.title, "The Lantern at North Pier");
    ASSERT_EQ(document.chapters.size(), 3U);
    EXPECT_EQ(document.chapters.front().title, "The Lantern");
    EXPECT_GT(document.word_count, 100U);

    const auto position = document.position_for_global_word(20);
    EXPECT_EQ(document.global_word_index(position), 20U);
    EXPECT_FALSE(document.text_for_words(position.chapter_index, position.word_index, 5).empty());
    EXPECT_EQ(document.chapters.front().words.front().character_offset, 0U);
}

TEST(reader_document_tests, parsesFb2MetadataAndParagraphs) {
    const auto document = goodreader::reader::ReaderDocument::open(sample_file("sample.fb2"));

    ASSERT_EQ(document.format, goodreader::domain::BookFormat::fb2);
    EXPECT_EQ(document.title, "The Lantern at North Pier");
    EXPECT_TRUE(document.author.empty());
    ASSERT_EQ(document.chapters.size(), 3U);
    EXPECT_EQ(document.chapters.front().title, "The Lantern");
    EXPECT_FALSE(document.chapters.front().paragraphs.empty());
}

TEST(reader_document_tests, extractsTextFromPdfAndReportsScannedFilesClearly) {
    const auto document = goodreader::reader::ReaderDocument::open(sample_file("sample.pdf"));
    ASSERT_EQ(document.format, goodreader::domain::BookFormat::pdf);
    ASSERT_EQ(document.chapters.size(), 1U);
    EXPECT_GT(document.word_count, 20U);

    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto scanned_path = std::filesystem::temp_directory_path() /
                              ("goodreader-scanned-" + std::to_string(suffix) + ".pdf");
    const ScopedFile cleanup{scanned_path};
    {
        std::ofstream output(scanned_path, std::ios::binary);
        output << "%PDF-1.4\n1 0 obj << /Type /Page >> endobj\n%%EOF";
    }

    try {
        static_cast<void>(goodreader::reader::ReaderDocument::open(scanned_path));
        FAIL() << "Scanned PDF without a text layer should be rejected.";
    } catch (const goodreader::reader::DocumentError& error) {
        EXPECT_EQ(error.code(), goodreader::reader::DocumentErrorCode::no_text_layer);
    }
}

} // namespace
