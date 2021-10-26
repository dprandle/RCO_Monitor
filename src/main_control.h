#pragma once

#define edm Main_Control::inst()

#include <inttypes.h>

const uint32_t MAX_SYSTEM_COUNT = 10;

class Subsystem;
class Timer;
class Logger;


class Main_Control
{
  public:
    Main_Control();
    virtual ~Main_Control();
    
    template<class T>
    T * add_subsystem()
    {
        T * sys = new T();
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

	void init();
	
    void release();

    void restart_updated(const char * exe_path, const char * const params[]);

    void start();

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
    Subsystem * systems_[MAX_SYSTEM_COUNT];
	Timer * m_systimer;
    Logger * logger_;
};