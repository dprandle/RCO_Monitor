#include <unistd.h>
#include <vector>
#include <sys/mount.h>

#include "utility.h"
#include "main_control.h"
#include "subsystem.h"
#include "timer.h"
#include "logger.h"
#include "config_file.h"

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

void Main_Control::init(Config_File * config)
{
    uint32_t len = util::buf_len(systems_);
    for (int i = 0; i < len; ++i)
        systems_[i]->init(config);
}

Main_Control & Main_Control::inst()
{
    static Main_Control controller;
    return controller;
}

void Main_Control::restart_updated(const char * exe_path, const char * const params[])
{
    uint32_t len = util::buf_len(params);
    char ** arr = (char **)malloc((len + 2) * sizeof(char *));

    arr[0] = (char *)exe_path;
    arr[len + 1] = nullptr;
    for (int i = 0; i < len; ++i)
        arr[i + 1] = (char *)params[i];

    ilog("Restarting {}", exe_path);
    stop();
    if (execv(exe_path, arr) == -1)
    {
        free(arr);
        elog("Could not restart {} - error: {}", exe_path, strerror(errno));
        start();
    }
}

bool Main_Control::thumb_drive_detected()
{
    return util::path_exists("/dev/sda");
}


const std::string & Main_Control::get_config_fname()
{
    return _config_fname;
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
            systems_[i] = systems_[len - 1];
            systems_[len - 1] = nullptr;
        }
    }
}

bool Main_Control::load_config(Config_File * cfg)
{
    // If a thumb drive is avialable, try to mount it
    if (thumb_drive_detected())
    {
        std::string mntpoint = "/media/usb0";
        if (mkdir(mntpoint.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0)
        {
            ilog("Created thumb drive mount point {} - attempting to mount", mntpoint);
            if (mount("/dev/sda1", mntpoint.c_str(), "vfat", MS_NOATIME, NULL) == 0)
            {
                ilog("Successfully mounted /dev/sda1 tp {}", mntpoint);
            }
            else
            {
                rmdir(mntpoint.c_str());
                wlog("Could not mount /dev/sda1 to {}: {}", mntpoint, strerror(errno));
            }
        }
        else
        {
            ilog("Could not create mount point {} for thumb drive: {}", mntpoint, strerror(errno));
        }
    }
    else
    {
        ilog("No thumb drive detected");
    }

    _config_fname = THUMB_DRIVE_MNT_DIR + "/config.json";
    bool loaded = cfg->load(_config_fname);
    if (!loaded)
    {
        wlog("Could not load config file {}: {} - trying backup path", _config_fname, strerror(errno));
        _config_fname = util::get_home_dir() + "/config.json";
        loaded = cfg->load(_config_fname);
        if (!loaded)
        {
            wlog("Also could not load config file at {}: {}", _config_fname, strerror(errno));
            _config_fname = util::get_exe_dir() + "/config.json";
            loaded = cfg->load(_config_fname);
            if (!loaded)
            {
                wlog("Aaand finally, could not load config file at {}: {} - no radio logging will happen without config!",
                     _config_fname,
                     strerror(errno));
                _config_fname = THUMB_DRIVE_MNT_DIR + "/config.json";
            }
        }
    }
    return loaded;
}

void Main_Control::start()
{
    logger_->initialize();
    ilog("Starting Radio Monitor");
    m_running = true;
    m_systimer->start();
    std::string default_config = THUMB_DRIVE_MNT_DIR + "/config.json";

    Config_File cfg;
    if (load_config(&cfg))
    {
        ilog("Successfully loaded config file at {}", _config_fname);
    }

    init(&cfg);
    while (running())
    {
        update();
    }
    m_systimer->stop();
    ilog("Stopping Radio Monitor - execution time {} ms", m_systimer->elapsed());
    release();
    logger_->terminate();
}

void Main_Control::stop()
{
    m_running = false;
}