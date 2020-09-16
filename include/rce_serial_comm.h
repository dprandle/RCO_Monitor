#pragma once

#include "subsystem.h"

const uint8_t TMP_BUF_SIZE = 96;
const uint8_t COMMAND_BUFFER_MAX_SIZE = 20;

const char update_firmware_id[] = "FWU";

class Uart;
class Timer;

class RCE_Serial_Comm : public Subsystem
{
  public:
    RCE_Serial_Comm();

    ~RCE_Serial_Comm();

    void init();

    void release();

    void update();

    std::string typestr();

    static std::string TypeString();

  private:
    void check_buffer_for_command_();

    Uart * rce_uart_;
    Timer * timer_;
    std::string command_buffer;
};