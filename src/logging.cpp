#include "reader/logging.hpp"

#include <memory>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace reader {

void init_logging() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("reader.log", true);

    auto logger = std::make_shared<spdlog::logger>(
        "reader", spdlog::sinks_init_list{console_sink, file_sink});

    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::err);

    spdlog::set_default_logger(logger);

    spdlog::info("Reader application started");
}

} // namespace reader
