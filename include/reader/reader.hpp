#pragma once

#include <string>

namespace reader {

class Reader {
  public:
    [[nodiscard]]
    std::string read(const std::string& path) const;
};

} // namespace reader
