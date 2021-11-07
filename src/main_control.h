#pragma once

#define edm Main_Control::inst()

#include <inttypes.h>
#include <string>

const std::string THUMB_DRIVE_MNT_DIR = "/media/usb0";
const uint32_t MAX_SYSTEM_COUNT = 10;

class Subsystem;
class Timer;
class Logger;
class Config_File;

class Main_Control
{
  public:
    Main_Control();
    virtual ~Main_Control();
    
    template<class T, class ...Args>
    T * add_subsystem(Args... args)
    {
        T * sys = new T(args...);
        if (add_subsystem(sys) == -1)
        {
            delete sys;
            return nullptr;
        }
        return sys;
    }

    int add_subsystem(Subsystem * subsys);

    static Main_Control & inst();

    bool running();

	void init(Config_File * config);
	
    void release();

    void restart_updated(const char * exe_path, const char * const params[]);

    void start(const std::string & config_fname);

    const std::string & get_config_fname();

	void stop();

	Timer * sys_timer();

    void update();
    
    template<class T>
    void remove_subsystem()
    {
        remove_subsystem(T::TypeString());
    }

    void remove_subsystem(const char * sysname);

    template<class T>
    T * get_subsystem()
    {
        return static_cast<T*>(get_subsystem(T::TypeString()));
    }

    Subsystem * get_subsystem(const char * sysname);
    
  private:
    bool m_running;
    std::string _config_fname;
    Subsystem * systems_[MAX_SYSTEM_COUNT];
	Timer * m_systimer;
    Logger * logger_;
};