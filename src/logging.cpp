#include "reader/logging.hpp"

namespace reader::logging {

void initialize() {
    spdlog::set_level(spdlog::level::debug);

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");

    spdlog::info("Logging initialized");
}

} // namespace reader::logging
