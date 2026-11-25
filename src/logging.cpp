#include "reader/logging.hpp"

#include <filesystem>
#include <memory>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace reader::logging {

void initialize() {
    // Создаём каталог для логов
    std::filesystem::create_directories("logs");

    // Console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    // File sink
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/reader.log",
                                                                            5 * 1024 * 1024, 3);

    console_sink->set_level(spdlog::level::debug);
    file_sink->set_level(spdlog::level::trace);

    console_sink->set_pattern("[%H:%M:%S] [%^%l%$] %v");

    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v");

    // Общий logger
    auto logger = std::make_shared<spdlog::logger>(
        "reader", spdlog::sinks_init_list{console_sink, file_sink});

    logger->set_level(spdlog::level::debug);

    spdlog::set_default_logger(logger);

    spdlog::info("Logging initialized");
}

} // namespace reader::logging
