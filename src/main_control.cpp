#include <unistd.h>
#include <vector>

#include "utility.h"
#include "main_control.h"
#include "subsystem.h"
#include "timer.h"
#include "logger.h"

Main_Control::Main_Control() : m_running(false), m_systimer(new Timer()), logger_(new Logger)
{
    util::zero_buf(systems_, MAX_SYSTEM_COUNT);
}

Main_Control::~Main_Control()
{
    delete m_systimer;
    uint32_t len = util::buf_len(systems_);
    for (int i = 0; i < len; ++i)
        delete systems_[i];
    util::zero_buf(systems_, MAX_SYSTEM_COUNT);
    delete logger_;
}

void Main_Control::init()
{
    logger_->initialize();
    uint32_t len = util::buf_len(systems_);
    for (int i = 0; i < len; ++i)
        systems_[i]->init();
}

Main_Control & Main_Control::inst()
{
    static Main_Control controller;
    return controller;
}

void Main_Control::restart_updated(const char * exe_path, const char * const params[])
{
    uint32_t len = util::buf_len(params);
    char** arr = (char**)malloc((len+2) * sizeof(char*));

    arr[0] = (char*)exe_path;
    arr[len+1] = nullptr;
    for (int i = 0; i < len; ++i)
        arr[i+1] = (char*)params[i];

    ilog("Restarting {}", exe_path);
    stop();
    if (execv(exe_path, arr) == -1)
    {
        free(arr);
        elog("Could not restart {} - error: {}",exe_path, strerror(errno));
        start();
    }
}

bool Main_Control::running()
{
    return m_running;
}

void Main_Control::release()
{
    uint32_t len = util::buf_len(systems_);
    for (int i = 0; i < len; ++i)
        systems_[i]->release();
    logger_->terminate();
}

void Main_Control::update()
{
    m_systimer->update();
    uint32_t len = util::buf_len(systems_);
    for (int i = 0; i < len; ++i)
        systems_[i]->update();
}

Timer * Main_Control::sys_timer()
{
    return m_systimer;
}

int Main_Control::add_subsystem(Subsystem * subsys)
{
    uint32_t len = util::buf_len(systems_, MAX_SYSTEM_COUNT);
    if (len >= MAX_SYSTEM_COUNT)
    {
        elog("Cannot add subsystem {} as max limit reached - raise subsystem limit", subsys->typestr());
        return -1;
    }
    systems_[len] = subsys;
    return len;
}

Subsystem * Main_Control::get_subsystem(const char * sysname)
{
    uint32_t len = util::buf_len(systems_);
    for (int i = 0; i < len; ++i)
    {
        const char * sys_name = systems_[i]->typestr();
        if (strcmp(sys_name, sysname) == 0)
            return systems_[i];
    }
    return nullptr;
}

void Main_Control::remove_subsystem(const char * sysname)
{
    uint32_t len = util::buf_len(systems_);
    for (int i = 0; i < len; ++i)
    {
        const char * sys_name = systems_[i]->typestr();
        if (strcmp(sys_name, sysname) == 0)
        {
            systems_[i]->release();
            delete systems_[i];
            systems_[i] = systems_[len-1];
            systems_[len-1] = nullptr;
        }
    }
}

void Main_Control::start()
{
    ilog("Starting RCO Monitor");
    m_running = true;
    m_systimer->start();

    init();
    while (running())
        update();
}

void Main_Control::stop()
{
    m_systimer->stop();
    ilog("Stopping RCO Monitor - execution time {} ms", m_systimer->elapsed());
    m_running = false;
    release();
}