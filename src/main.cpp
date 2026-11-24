#include "reader/reader.hpp"

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: reader <file>\n";
        return 1;
    }

    try {
        reader::Reader reader;
        std::cout << reader.read(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
