#include "goodreader/core_c_api.h"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace {

class core_c_api_tests : public ::testing::Test {
  protected:
    void SetUp() override {
        ASSERT_EQ(gr_core_create(&core_), GR_STATUS_OK);
        ASSERT_NE(core_, nullptr);
        directory_ = std::filesystem::temp_directory_path() / "goodreader_c_api_tests";
        std::filesystem::create_directories(directory_);
    }

    void TearDown() override {
        gr_core_destroy(core_);
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    std::filesystem::path create_file(const std::string& name) {
        const auto path = directory_ / name;
        std::ofstream file(path);
        file << "user-provided content";
        file.close();
        return path;
    }

    gr_core_t* core_ = nullptr;
    std::filesystem::path directory_;
};

struct callback_result {
    std::mutex mutex;
    std::condition_variable changed;
    bool completed = false;
    gr_status_t status = GR_STATUS_INTERNAL_ERROR;
    gr_book_format_t format = GR_BOOK_FORMAT_UNKNOWN;
    std::string detail;
};

void on_validation_complete(gr_status_t status, gr_book_format_t format, const char* detail,
                            void* user_data) {
    auto* result = static_cast<callback_result*>(user_data);
    {
        const std::lock_guard lock(result->mutex);
        result->status = status;
        result->format = format;
        result->detail = detail == nullptr ? "" : detail;
        result->completed = true;
    }
    result->changed.notify_one();
}

TEST_F(core_c_api_tests, exposesStableVersionAndCreatesAnInstance) {
    EXPECT_STREQ(gr_core_version(), "0.1.0");
    EXPECT_EQ(gr_core_create(nullptr), GR_STATUS_INVALID_ARGUMENT);
}

TEST_F(core_c_api_tests, validatesSupportedFormatsCaseInsensitively) {
    const auto path = create_file("Sample.Fb2");
    auto format = GR_BOOK_FORMAT_UNKNOWN;

    const auto status = gr_core_validate_book_file(core_, path.string().c_str(), &format);

    EXPECT_EQ(status, GR_STATUS_OK);
    EXPECT_EQ(format, GR_BOOK_FORMAT_FB2);
}

TEST_F(core_c_api_tests, rejectsUnsupportedFormatsAndMissingFiles) {
    const auto unsupported = create_file("notes.txt");
    const auto missing = directory_ / "missing.epub";
    auto format = GR_BOOK_FORMAT_PDF;

    EXPECT_EQ(gr_core_validate_book_file(core_, unsupported.string().c_str(), &format),
              GR_STATUS_UNSUPPORTED_FORMAT);
    EXPECT_EQ(format, GR_BOOK_FORMAT_UNKNOWN);
    EXPECT_EQ(gr_core_validate_book_file(core_, missing.string().c_str(), &format),
              GR_STATUS_NOT_FOUND);
}

TEST_F(core_c_api_tests, rejectsInvalidSynchronousArguments) {
    const auto path = create_file("book.pdf");
    auto format = GR_BOOK_FORMAT_UNKNOWN;

    EXPECT_EQ(gr_core_validate_book_file(nullptr, path.string().c_str(), &format),
              GR_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(gr_core_validate_book_file(core_, nullptr, &format), GR_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(gr_core_validate_book_file(core_, path.string().c_str(), nullptr),
              GR_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(format, GR_BOOK_FORMAT_UNKNOWN);
}

TEST_F(core_c_api_tests, opensReflowableBookContentAndExposesStableWordPositions) {
    const auto path = std::filesystem::path(GOODREADER_TEST_DATA_DIR) / "sample.epub";
    gr_book_document_t* document = nullptr;
    std::array<char, 256> detail{};
    std::size_t detail_required = 0;

    ASSERT_EQ(gr_core_open_book_file(core_, path.string().c_str(), &document, detail.data(),
                                     detail.size(), &detail_required),
              GR_STATUS_OK)
        << detail.data();
    ASSERT_NE(document, nullptr);

    std::size_t chapter_count = 0;
    std::size_t word_count = 0;
    ASSERT_EQ(gr_book_document_get_counts(document, &chapter_count, &word_count), GR_STATUS_OK);
    EXPECT_EQ(chapter_count, 3U);
    EXPECT_GT(word_count, 100U);

    std::array<char, 128> title{};
    std::array<char, 128> author{};
    std::size_t title_required = 0;
    std::size_t author_required = 0;
    ASSERT_EQ(gr_book_document_copy_metadata(document, title.data(), title.size(), &title_required,
                                             author.data(), author.size(), &author_required),
              GR_STATUS_OK);
    EXPECT_STREQ(title.data(), "The Lantern at North Pier");

    std::array<char, 64> chapter_title{};
    std::size_t chapter_title_required = 0;
    std::size_t chapter_words = 0;
    ASSERT_EQ(gr_book_document_copy_chapter_title(document, 0, chapter_title.data(),
                                                  chapter_title.size(), &chapter_title_required,
                                                  &chapter_words),
              GR_STATUS_OK);
    EXPECT_STREQ(chapter_title.data(), "The Lantern");
    EXPECT_GT(chapter_words, 0U);

    gr_text_position_t position{};
    ASSERT_EQ(gr_book_document_position_for_global_word(document, 20, &position), GR_STATUS_OK);
    std::size_t global_word = 0;
    ASSERT_EQ(gr_book_document_global_word_index(document, position, &global_word), GR_STATUS_OK);
    EXPECT_EQ(global_word, 20U);

    std::array<char, 128> range{};
    std::size_t range_required = 0;
    ASSERT_EQ(gr_book_document_copy_word_range(document, position.chapter_index,
                                               position.word_index, 5, range.data(), range.size(),
                                               &range_required),
              GR_STATUS_OK);
    EXPECT_GT(range_required, 1U);
    EXPECT_NE(range[0], '\0');
    gr_book_document_destroy(document);
}

TEST_F(core_c_api_tests, reportsScannedPdfWithoutASelectableTextLayer) {
    const auto path = create_file("scanned.pdf");
    std::ofstream(path, std::ios::binary | std::ios::trunc)
        << "%PDF-1.4\n1 0 obj << /Type /Page >> endobj\n%%EOF";
    gr_book_document_t* document = nullptr;
    std::array<char, 256> detail{};
    std::size_t detail_required = 0;

    EXPECT_EQ(gr_core_open_book_file(core_, path.string().c_str(), &document, detail.data(),
                                     detail.size(), &detail_required),
              GR_STATUS_NO_TEXT_LAYER);
    EXPECT_EQ(document, nullptr);
    EXPECT_NE(std::string(detail.data()).find("Scanned"), std::string::npos);
}

TEST_F(core_c_api_tests, mapsStableTextPositionsToAndFromCumulativeAudioTime) {
    const auto path = std::filesystem::path(GOODREADER_TEST_DATA_DIR) / "sample.epub";
    gr_book_document_t* document = nullptr;
    ASSERT_EQ(gr_core_open_book_file(core_, path.string().c_str(), &document, nullptr, 0, nullptr),
              GR_STATUS_OK);
    ASSERT_NE(document, nullptr);
    std::size_t chapter_count = 0;
    std::size_t word_count = 0;
    ASSERT_EQ(gr_book_document_get_counts(document, &chapter_count, &word_count), GR_STATUS_OK);
    ASSERT_GT(word_count, 2U);

    const std::array<gr_alignment_anchor_t, 2> anchors{{
        {0, 0, 1.0},
        {word_count - 1, 120'000, 0.9},
    }};
    gr_alignment_index_t* index = nullptr;
    ASSERT_EQ(gr_alignment_index_create(anchors.data(), anchors.size(), &index), GR_STATUS_OK);
    ASSERT_NE(index, nullptr);

    gr_text_position_t position{};
    ASSERT_EQ(gr_book_document_position_at_audio_time(document, index, 60'000, &position),
              GR_STATUS_OK);
    std::size_t global_word = 0;
    ASSERT_EQ(gr_book_document_global_word_index(document, position, &global_word), GR_STATUS_OK);
    EXPECT_EQ(global_word, (word_count - 1) / 2);

    std::int64_t audio_time = -1;
    ASSERT_EQ(gr_book_document_audio_time_for_position(document, index, position, &audio_time),
              GR_STATUS_OK);
    EXPECT_NEAR(audio_time, 60'000, 120'000.0 / static_cast<double>(word_count - 1));

    gr_alignment_index_destroy(index);
    gr_book_document_destroy(document);
}

TEST_F(core_c_api_tests, rejectsNonMonotonicAlignmentAnchors) {
    const std::array<gr_alignment_anchor_t, 2> anchors{{
        {10, 20, 1.0},
        {9, 30, 0.9},
    }};
    gr_alignment_index_t* index = nullptr;
    EXPECT_EQ(gr_alignment_index_create(anchors.data(), anchors.size(), &index),
              GR_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(index, nullptr);
}

TEST_F(core_c_api_tests, invokesAsyncValidationCallbackWithTheDetectedFormat) {
    const auto path = create_file("book.epub");
    callback_result result;
    gr_operation_t* operation = nullptr;

    ASSERT_EQ(gr_core_validate_book_file_async(core_, path.string().c_str(), on_validation_complete,
                                               &result, &operation),
              GR_STATUS_OK);
    ASSERT_NE(operation, nullptr);
    {
        std::unique_lock lock(result.mutex);
        ASSERT_TRUE(result.changed.wait_for(lock, std::chrono::seconds(3),
                                            [&result] { return result.completed; }));
    }

    EXPECT_EQ(result.status, GR_STATUS_OK);
    EXPECT_EQ(result.format, GR_BOOK_FORMAT_EPUB);
    gr_operation_release(operation);
}

TEST_F(core_c_api_tests, acceptsCancellationRequestsForActiveOperations) {
    const auto path = create_file("book.pdf");
    callback_result result;
    gr_operation_t* operation = nullptr;

    ASSERT_EQ(gr_core_validate_book_file_async(core_, path.string().c_str(), on_validation_complete,
                                               &result, &operation),
              GR_STATUS_OK);
    ASSERT_NE(operation, nullptr);
    EXPECT_EQ(gr_operation_cancel(operation), GR_STATUS_OK);
    EXPECT_EQ(gr_operation_cancel(nullptr), GR_STATUS_INVALID_ARGUMENT);

    {
        std::unique_lock lock(result.mutex);
        ASSERT_TRUE(result.changed.wait_for(lock, std::chrono::seconds(3),
                                            [&result] { return result.completed; }));
    }
    EXPECT_TRUE(result.status == GR_STATUS_OK || result.status == GR_STATUS_CANCELLED);
    gr_operation_release(operation);
}

} // namespace
