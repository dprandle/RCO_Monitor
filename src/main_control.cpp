#include <utility.h>
#include <main_control.h>
#include <subsystem.h>
#include <string>
#include <vector>
#include <timer.h>
#include <logger.h>

Main_Control::Main_Control():
	m_running(false),
	m_systimer(new Timer())
{
	
}

Main_Control::~Main_Control()
{
	delete m_systimer;
    sysmap::iterator sysiter = m_systems.begin();
    while (sysiter != m_systems.end())
    {
        delete sysiter->second;
        ++sysiter;
    }
}

void Main_Control::init()
{
    m_logger.initialize();
    sysmap::iterator sysiter = m_systems.begin();
    while (sysiter != m_systems.end())
    {
        sysiter->second->init();
        ++sysiter;
    }
}

Main_Control & Main_Control::inst()
{
    static Main_Control controller;
    return controller;
}

bool Main_Control::running()
{
    return m_running;
}

void Main_Control::release()
{
	sysmap::iterator sysiter = m_systems.begin();
    while (sysiter != m_systems.end())
    {
		sysiter->second->release();
        ++sysiter;
    }
    m_logger.terminate();
}

void Main_Control::update()
{
	m_systimer->update();
    sysmap::iterator sysiter = m_systems.begin();
    while (sysiter != m_systems.end())
    {
        sysiter->second->update();
        ++sysiter;
    }
}

Timer * Main_Control::sys_timer()
{
	return m_systimer;
}

Subsystem * Main_Control::sys(const std::string & sysname)
{
    sysmap::iterator iter = m_systems.find(sysname);
    if (iter != m_systems.end())
        return iter->second;
    return NULL;
}

void Main_Control::rm_sys(const std::string & sysname)
{
    sysmap::iterator iter = m_systems.find(sysname);
    if (iter != m_systems.end())
    {
        delete iter->second;
        m_systems.erase(iter);
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
}

void Main_Control::quit(void)
{
	edm.stop();
	edm.release();
}
