#pragma once

#include <spdlog/spdlog.h>

#define tlog SPDLOG_TRACE
#define dlog SPDLOG_DEBUG
#define ilog SPDLOG_INFO
#define wlog SPDLOG_WARN
#define elog SPDLOG_ERROR
#define clog SPDLOG_CRITICAL

namespace spdlog
{
class logger;
}

class edlogger
{
  public:
    edlogger();
    ~edlogger();

    void initialize();

    void terminate();

  private:
    std::shared_ptr<spdlog::logger> logger;
};