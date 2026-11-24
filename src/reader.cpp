#include "reader/reader.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace reader {

std::string Reader::read(const std::string& path) const {
    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

} // namespace reader
