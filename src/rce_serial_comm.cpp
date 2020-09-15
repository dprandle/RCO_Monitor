#include "rce_serial_comm.h"
#include "uart.h"
#include "utility.h"
#include "logger.h"


RCE_Serial_Comm::RCE_Serial_Comm() : rce_uart_(new Uart(Uart::Uart1))
{}

RCE_Serial_Comm::~RCE_Serial_Comm()
{
    delete rce_uart_;
}

void RCE_Serial_Comm::init()
{
    rce_uart_->set_baud(Uart::b9600);
    Uart::DataFormat df;
    df.db = Uart::d8;
    df.p = Uart::None;
    df.sb = Uart::One;
    rce_uart_->set_format(df);

    rce_uart_->start();
    Subsystem::init();
}

void RCE_Serial_Comm::release()
{
    rce_uart_->stop();
    Subsystem::release();
}

void RCE_Serial_Comm::update()
{
    std::string cur_str;

    uint32_t cnt = rce_uart_->read(command_buffer, COMMAND_BUFFER_SIZE);
    cur_str.resize(cnt);
    for (int i = 0; i < cnt; ++i)
        cur_str[i] = command_buffer[i];
    
    if (cnt > 0)
        dlog("Received string {}",cur_str);
    
    rce_uart_->write(command_buffer, cnt);
}

std::string RCE_Serial_Comm::typestr()
{
    return TypeString();
}

std::string RCE_Serial_Comm::TypeString()
{
    return "RCE_Serial_Comm";
}
