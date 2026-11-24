#pragma once

#include <string>

namespace reader {

class Reader {
  public:
    Reader() = default;

    [[nodiscard]]
    std::string read(const std::string& path) const;
};

} // namespace reader
