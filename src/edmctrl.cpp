/*!
  \file   edmctrl.cpp
  \author Daniel <dprandle@dprandle-CZ-17>
  \date   Fri Jul 10 09:19:32 2015
  
  \brief  Master control file for the edison
  
  
*/

#include <edutility.h>
#include <edmctrl.h>
#include <edsystem.h>
#include <string>
#include <vector>
#include <edtimer.h>
#include <edlogger.h>

edmctrl::edmctrl():
	m_running(false),
	m_systimer(new edtimer())
{
	
}

edmctrl::~edmctrl()
{
	delete m_systimer;
    sysmap::iterator sysiter = m_systems.begin();
    while (sysiter != m_systems.end())
    {
        delete sysiter->second;
        ++sysiter;
    }
}

void edmctrl::init()
{
    m_logger.initialize();
    sysmap::iterator sysiter = m_systems.begin();
    while (sysiter != m_systems.end())
    {
        ilog("Initializing system {}", sysiter->first);
        sysiter->second->init();
        ++sysiter;
    }
}

edmctrl & edmctrl::inst()
{
    static edmctrl controller;
    return controller;
}

bool edmctrl::running()
{
    return m_running;
}

void edmctrl::release()
{
    //log_message("Releasing edison control engine");
	sysmap::iterator sysiter = m_systems.begin();
    while (sysiter != m_systems.end())
    {
        ilog("Releasing system {}",sysiter->first);
		sysiter->second->release();
        ++sysiter;
    }
    m_logger.terminate();
}

void edmctrl::update()
{
	m_systimer->update();
    sysmap::iterator sysiter = m_systems.begin();
    while (sysiter != m_systems.end())
    {
        sysiter->second->update();
        ++sysiter;
    }
}

edtimer * edmctrl::sys_timer()
{
	return m_systimer;
}

edsystem * edmctrl::sys(const std::string & sysname)
{
    sysmap::iterator iter = m_systems.find(sysname);
    if (iter != m_systems.end())
        return iter->second;
    return NULL;
}

void edmctrl::rm_sys(const std::string & sysname)
{
    sysmap::iterator iter = m_systems.find(sysname);
    if (iter != m_systems.end())
    {
        delete iter->second;
        m_systems.erase(iter);
    }
}

void edmctrl::start()
{
	ilog("Starting light controller");
	m_running = true;
	m_systimer->start();
}

void edmctrl::stop()
{
	
	m_systimer->stop();
    ilog("Stopping light controller - execution time {} ms", m_systimer->elapsed());
	m_running = false;
}

void edmctrl::quit(void)
{
	edm.stop();
	edm.release();
}
