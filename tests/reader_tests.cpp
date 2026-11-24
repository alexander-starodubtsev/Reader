#include "reader/reader.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <stdexcept>

int main() {
    try {
        reader::Reader reader{"Example"};
        reader.show();
    } catch (const std::runtime_error&) {
        exception_thrown = true;
    }
    assert(exception_thrown);
}
