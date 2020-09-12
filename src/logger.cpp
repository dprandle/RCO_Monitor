#include <logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

Logger::Logger()
{}

Logger::~Logger()
{}

void Logger::initialize()
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/multisink.txt", true);
    file_sink->set_level(spdlog::level::trace);

    spdlog::sinks_init_list sink_list = {file_sink, console_sink};

    logger_ = std::make_shared<spdlog::logger>("multi_sink", sink_list.begin(), sink_list.end());
    logger_->set_level(spdlog::level::trace);
    logger_->set_pattern("[%m/%d %X.%e TID:%t] [%s:%#] %^[%l]%$ %v");
    spdlog::set_default_logger(logger_);
}

void Logger::terminate()
{
    logger_->flush();
    spdlog::shutdown();
}