#pragma once

#include "subsystem.h"

class Socket;

class Radio_Telnet : public Subsystem
{
  public:
    Radio_Telnet();

    ~Radio_Telnet();

    void init();

    void release();

    void update();

    const char * typestr();

    static const char * TypeString();

  private:
    
    Socket * rad_rx;
    
};