#pragma once

#include <string>
#include <map>
#include <logger.h>

#define edm Main_Control::inst()

class Subsystem;
class Timer;

typedef std::map<std::string,Subsystem*> sysmap;

class Main_Control
{
  public:
    Main_Control();
    virtual ~Main_Control();
    
    template<class T>
    T * add_sys()
    {
        T * sys = new T();
        std::pair<sysmap::iterator,bool> ret = m_systems.insert(std::pair<std::string,Subsystem*>(sys->typestr(), sys));
        if (!ret.second)
        {
            wlog("Could not add system {}",sys->typestr());
            delete sys;
            return NULL;
        }
        return sys;
    }

    static Main_Control & inst();

    bool running();

	void init();
	
    void release();

    void start();

	void stop();

	Timer * sys_timer();

    void update();
    
    template<class T>
    void rm_sys()
    {
        rm_sys(T::TypeString());
    }

    void rm_sys(const std::string & sysname);

    template<class T>
    T * sys()
    {
        return static_cast<T*>(sys(T::TypeString()));
    }

    Subsystem * sys(const std::string & sysname);

	static void quit(void);
    
  private:
    bool m_running;
    sysmap m_systems;
	Timer * m_systimer;
    Logger m_logger;
};