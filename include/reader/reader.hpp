#pragma once

#include <string>

namespace reader {

class Reader {
  private:
    std::string name;

  public:
    Reader(const std::string& str) : name(str){};
    void show() const;
};

} // namespace reader
