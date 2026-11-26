#include "reader/reader.hpp"
#include "reader/logging.hpp"

#include <fstream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>

namespace reader {

std::string Reader::read(const std::string& path) const {
    spdlog::debug("Opening file: {}", path);

    std::ifstream file(path);

    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", path);

        throw std::runtime_error("Cannot open file: " + path);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    if (file.bad()) {
        spdlog::error("Failed to read file: {}", path);

        throw std::runtime_error("Failed to read file: " + path);
    }

    auto content = buffer.str();

    spdlog::info("Successfully read {} bytes from {}", content.size(), path);

    return content;
}

} // namespace reader
