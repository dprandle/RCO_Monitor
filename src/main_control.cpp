#include <unistd.h>
#include <vector>
#include <sys/mount.h>

#include "utility.h"
#include "main_control.h"
#include "subsystem.h"
#include "timer.h"
#include "logger.h"
#include "config_file.h"

const int32_t mount_unmount_wait_ms = 4000;

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
    unmount_drive();
}

void Main_Control::unmount_drive()
{
    // if (system(("umount " + THUMB_DRIVE_MNT_DIR).c_str()) == 0)
    // {
    //     ilog("Successfully unmounted drive from {} - waiting {}ms", THUMB_DRIVE_MNT_DIR, mount_unmount_wait_ms);
    //     usleep(mount_unmount_wait_ms*1000);
    // }
    errno = 0;
    if (umount(THUMB_DRIVE_MNT_DIR.c_str()) == 0)
    {
        ilog("Unmounted {}",THUMB_DRIVE_MNT_DIR);
    }
    else
    {
        ilog("Did not unmount {}: {}", THUMB_DRIVE_MNT_DIR, strerror(errno));
    }
    rmdir(THUMB_DRIVE_MNT_DIR.c_str());
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

void Main_Control::mount_drive()
{
    mkdir("/media", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    // If a thumb drive is avialable, try to mount it
    if (thumb_drive_detected())
    {
        errno = 0;
        std::string mntpoint = THUMB_DRIVE_MNT_DIR;
        if (mkdir(mntpoint.c_str(), 0777) == 0 || errno == EEXIST)
        {
            if (errno == EEXIST)
            {
                ilog("Mount dir {} already exists - attempting to mount", mntpoint);
            }
            else
            {
                ilog("Created thumb drive mount dir {} - attempting to mount", mntpoint);
            }

            // Have to use cmd line tool here - no matter what i do sys call mount() doesn't work!
            int max_retry_count = 50;
            int cur_retry_count = 0;
            std::string cmd("mount -o sync /dev/sda1 " + mntpoint);
            while (system(cmd.c_str()) != 0 && cur_retry_count != max_retry_count)
            {
                ilog("Retrying mounted thumb drive at /dev/sda1 to {}", mntpoint);
                ++cur_retry_count;
            }

            if (cur_retry_count == max_retry_count)
            {
                rmdir(mntpoint.c_str());
                wlog("Could not mount /dev/sda1 to {} - reached max retry count for mounting procedure", mntpoint);
            }
            else
            {
                ilog("Successfully mounted device at /dev/sda1 to {} - waiting {}ms...", mntpoint,mount_unmount_wait_ms);
                usleep(1000*mount_unmount_wait_ms);
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
}

bool Main_Control::load_config(Config_File * cfg)
{
    _config_fname = THUMB_DRIVE_MNT_DIR + "/config.json";
    bool loaded = cfg->load(_config_fname);
    if (!loaded)
    {
        wlog("Could not load config file {}: {} - trying backup path", _config_fname, strerror(errno));
        std::string home_dir = util::get_home_dir({"ubuntu", "dprandle", "root"});
        _config_fname = home_dir + "/config.json";
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
    unmount_drive();
    mount_drive();
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