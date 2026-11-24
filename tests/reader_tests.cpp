#include "reader/reader.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <stdexcept>

int main() {
    const char* path = "reader_test_file.txt";

    {
        std::ofstream file(path);
        assert(file.is_open());
        file << "Hello, Reader!";
    }

    reader::Reader reader;

    const auto content = reader.read(path);
    assert(content == "Hello, Reader!");

    std::remove(path);

    bool exception_thrown = false;

    try {
        (void)reader.read("nonexistent_reader_file.txt");
    } catch (const std::runtime_error&) {
        exception_thrown = true;
    }

    assert(exception_thrown);
}
