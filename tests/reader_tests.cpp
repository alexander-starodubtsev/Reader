#include "reader/reader.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

class ReaderTests : public ::testing::Test {
  protected:
    void SetUp() override {
        test_directory = std::filesystem::temp_directory_path() / "reader_unit_tests";

        std::filesystem::create_directories(test_directory);
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(test_directory, error);
    }

    std::filesystem::path create_file(const std::string& name, const std::string& content) {
        const auto path = test_directory / name;

        std::ofstream file(path);
        file << content;
        file.close();

        return path;
    }

    std::filesystem::path test_directory;
    reader::Reader reader;
};

TEST_F(ReaderTests, ReadsFileSuccessfully) {
    const auto path = create_file("test.txt", "Hello, Reader!");

    const auto content = reader.read(path.string());

    EXPECT_EQ(content, "Hello, Reader!");
}

TEST_F(ReaderTests, ReadsMultilineFile) {
    const auto path = create_file("multiline.txt", "First line\nSecond line\nThird line");

    const auto content = reader.read(path.string());

    EXPECT_EQ(content, "First line\nSecond line\nThird line");
}

TEST_F(ReaderTests, ReadsEmptyFile) {
    const auto path = create_file("empty.txt", "");

    const auto content = reader.read(path.string());

    EXPECT_TRUE(content.empty());
}

TEST_F(ReaderTests, ThrowsWhenFileDoesNotExist) {
    const auto path = test_directory / "nonexistent.txt";

    EXPECT_THROW(static_cast<void>(reader.read(path.string())), std::runtime_error);
}

TEST_F(ReaderTests, ReadsLargeFile) {
    const std::string expected(1024 * 1024, 'A');

    const auto path = create_file("large.txt", expected);

    const auto content = reader.read(path.string());

    EXPECT_EQ(content.size(), expected.size());
    EXPECT_EQ(content, expected);
}

} // namespace
