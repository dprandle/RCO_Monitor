#pragma once

#include "subsystem.h"

const uint8_t COMMAND_BUFFER_SIZE = 255;

class Uart;
class Timer;

class RCE_Serial_Comm : public Subsystem
{
  public:
    RCE_Serial_Comm();

    virtual ~RCE_Serial_Comm();

    virtual void init();

    virtual void release();

    virtual void update();

    virtual std::string typestr();

    static std::string TypeString();

  private:
    Uart * rce_uart_;
    Timer * timer_;
    uint8_t command_buffer[COMMAND_BUFFER_SIZE];
};