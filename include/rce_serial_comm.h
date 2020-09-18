#pragma once

#include <map>
#include <vector>

#include "shared_structs.h"
#include "subsystem.h"


const uint8_t TMP_BUF_SIZE = 96;
const uint8_t COMMAND_BUFFER_MAX_SIZE = 20;

class Uart;
class Timer;


/// Subclass this and override process in order to register a new Uart command
struct Command_Handler
{
    Command_Handler()
    {}

    virtual ~Command_Handler()
    {}

    virtual bool process(uint8_t * buffer, uint32_t size) = 0;

    Uart * rce_uart;
};

struct Firmware_Update : public Command_Handler
{
    bool process(uint8_t * buffer, uint32_t size);
    Firmware_Header hdr;
    uint32_t current_ind;
    std::vector<uint8_t> payload;
};

class RCE_Serial_Comm : public Subsystem
{
  public:
    RCE_Serial_Comm();

    ~RCE_Serial_Comm();

    void init();

    void release();

    void update();

    template<class T>
    T * add_command(const std::string & command)
    {
        T * handler = new T;
        handler->rce_uart = rce_uart_;
        auto ins_iter = command_handlers_.emplace(command, handler);
        if (ins_iter.second)
            return handler;
        return nullptr;
    }

    Command_Handler * get_command(const std::string & command);

    bool remove_command(const std::string & command);

    std::string typestr();

    static std::string TypeString();

  private:
    void check_buffer_for_command_();

    Uart * rce_uart_;

    std::string current_command;

    std::string command_buffer;

    std::map<std::string, Command_Handler *> command_handlers_;
};