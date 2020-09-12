#include <edlogger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

edlogger::edlogger()
{}

edlogger::~edlogger()
{}

void edlogger::initialize()
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/multisink.txt", true);
    file_sink->set_level(spdlog::level::trace);

    spdlog::sinks_init_list sink_list = {file_sink, console_sink};

    logger = std::make_shared<spdlog::logger>("multi_sink", sink_list.begin(), sink_list.end());
    logger->set_level(spdlog::level::trace);
    logger->set_pattern("[%m/%d %X.%e TID:%t] [%s:%#] %^[%l]%$ %v");
    spdlog::set_default_logger(logger);
}

void edlogger::terminate()
{
    logger->flush();
    spdlog::shutdown();
}