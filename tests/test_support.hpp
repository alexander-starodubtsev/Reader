#pragma once

#include "goodreader/domain/types.hpp"

#include <date/date.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace goodreader::tests {

inline domain::Timestamp parse_timestamp(const std::string& value) {
    std::istringstream input(value);
    domain::Timestamp parsed;
    input >> date::parse("%FT%TZ", parsed);
    if (input.fail()) {
        throw std::invalid_argument("Invalid test timestamp: " + value);
    }
    return parsed;
}

inline std::string read_fixture(const std::string& name) {
    const std::string path = std::string(GOODREADER_FIXTURES_DIR) + "/" + name;
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to read test fixture: " + path);
    }
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

} // namespace goodreader::tests
