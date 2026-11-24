#include "reader/reader.hpp"

#include <ostream>

namespace reader {

void Reader::show() const { std::cout << name << std::endl; }

} // namespace reader
