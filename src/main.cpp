#include "reader/logging.hpp"
#include "reader/reader.hpp"

#include <exception>
#include <iostream>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    reader::logging::initialize();

    if (argc < 2) {
        spdlog::error("Usage: reader <file_path>");
        return 1;
    }

    try {
        reader::Reader file_reader;

        const auto content = file_reader.read(argv[1]);

        std::cout << "========== FILE CONTENT ==========\n";
        std::cout << content;

        if (content.empty() || content.back() != '\n') {
            std::cout << '\n';
        }

        std::cout << "==================================\n";

        spdlog::info("Application finished successfully");

        return 0;
    } catch (const std::exception& error) {
        spdlog::critical("Application error: {}", error.what());
        return 1;
    }
}
