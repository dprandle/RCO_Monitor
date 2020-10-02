#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "logger.h"

Logger::Logger():initialized(false)
{}

Logger::~Logger()
{}

void Logger::initialize()
{
    if (!initialized)
    {
        initialized = true;
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::debug);

        auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>("logs/rco_monitor.log", 0, 0);
        file_sink->set_level(spdlog::level::trace);

        spdlog::sinks_init_list sink_list = {file_sink, console_sink};

        spdlog::flush_every(std::chrono::seconds(3));

        logger_ = std::make_shared<spdlog::logger>("multi_sink", sink_list.begin(), sink_list.end());
        logger_->set_level(spdlog::level::trace);
        logger_->set_pattern("[%m/%d %X.%e TID:%t] [%s:%#] %^[%l]%$ %v");
        spdlog::set_default_logger(logger_);
    }
}

void Logger::terminate()
{
    logger_->flush();
}