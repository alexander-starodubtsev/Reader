#include "reader/reader.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        reader::Reader reader{"Book's name"};
        spdlog::info("File successfully made");
        reader.show();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
