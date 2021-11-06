#pragma once

#include "shared_structs.h"
#include "subsystem.h"
#include "utility.h"


const uint8_t TMP_BUF_SIZE = 96;
const uint8_t COMMAND_BUFFER_MAX_SIZE = 20;
const int32_t MAX_COMMAND_COUNT = 20;
const int16_t MAX_TIMEOUT_MS = 10000;

class Uart;
class Timer;

/// Subclass this and override process in order to register a new Uart command
struct Command_Handler
{
    Command_Handler():rce_uart(nullptr)
    {}

    virtual ~Command_Handler()
    {}

    virtual bool process(uint8_t * buffer, uint32_t size) = 0;

    Uart * rce_uart;

    char command[COMMAND_BUFFER_MAX_SIZE];
};

struct Firmware_Update : public Command_Handler
{
    Firmware_Update():Command_Handler(), hdr(),current_ind() {}
    
    bool process(uint8_t * buffer, uint32_t size);
    
    Firmware_Header hdr;
    
    uint32_t current_ind;
    
    uint8_t * payload;
};

struct Reboot_Updated_Firmware : public Command_Handler
{
    Reboot_Updated_Firmware():Command_Handler(), hdr(), current_ind(0) {}

    bool process(uint8_t * buffer, uint32_t size);
    Firmware_Header hdr;
    uint32_t current_ind;
};

struct Get_Firmware_Versions : public Command_Handler
{
    Get_Firmware_Versions():Command_Handler() {}  
    bool process(uint8_t * buffer, uint32_t size);
};


class RCE_Serial_Comm : public Subsystem
{
  public:
    RCE_Serial_Comm();

    ~RCE_Serial_Comm();

    void init(Config_File * config);

    void release();

    void update();

    template<class T>
    T * add_command(const char * command)
    {
        uint32_t cur_ind = util::buf_len(command_handlers_, MAX_COMMAND_COUNT);
        
        if (cur_ind == MAX_COMMAND_COUNT)
            return nullptr;
        
        T * handler = static_cast<T*>(new T);
        handler->rce_uart = rce_uart_;
        strcpy(handler->command, command);
        command_handlers_[cur_ind] = handler;
        return handler;
    }

    template<class T>
    T * command(const char * command)
    {
        return static_cast<T*>(get_command(command));
    }

    Command_Handler * get_command(const char * command);

    const char * typestr();

    static const char * TypeString();

  private:
    void check_buffer_for_command_();

    Timer * reset_timer_;

    Uart * rce_uart_;

    char current_command[COMMAND_BUFFER_MAX_SIZE];

    char command_buffer[COMMAND_BUFFER_MAX_SIZE];

    Command_Handler * command_handlers_[MAX_COMMAND_COUNT];
};