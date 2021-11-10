#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <list>

#include "utility.h"
#include "logger.h"
#include "main_control.h"

Logger::Logger() : initialized(false)
{}

Logger::~Logger()
{}

void Logger::initialize()
{
    if (!initialized)
    {
        std::string home_dir = util::get_home_dir({"ubuntu", "dprandle", "root"});
        std::string local_dir = home_dir + "/status_logs";
        std::string log_fname("/radio_monitor.log");

        initialized = true;
        std::vector<spdlog::sink_ptr> loggers;

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::debug);
        loggers.push_back(console_sink);

        if (util::path_exists(local_dir) || (mkdir(local_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0))
        {
            auto file_sink_local = std::make_shared<spdlog::sinks::daily_file_sink_mt>(local_dir + log_fname, 0, 0);
            file_sink_local->set_level(spdlog::level::info);
            loggers.push_back(file_sink_local);
        }

        spdlog::flush_every(std::chrono::seconds(3));
        spdlog::flush_on(spdlog::level::info);

        logger_ = std::make_shared<spdlog::logger>("multi_sink", loggers.begin(), loggers.end());
        logger_->set_level(spdlog::level::trace);
        logger_->set_pattern("[%m/%d %X.%e TID:%t] [%s:%#] %^[%l]%$ %v");
        spdlog::set_default_logger(logger_);
    }
}

void Logger::terminate()
{
    logger_->flush();
}