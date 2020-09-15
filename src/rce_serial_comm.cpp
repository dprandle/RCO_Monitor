#include "rce_serial_comm.h"
#include "uart.h"
#include "utility.h"
#include "logger.h"
#include "timer.h"

RCE_Serial_Comm::RCE_Serial_Comm() : rce_uart_(new Uart(Uart::Uart1)), timer_(new Timer())
{}

RCE_Serial_Comm::~RCE_Serial_Comm()
{
    delete rce_uart_;
    delete timer_;
}

void RCE_Serial_Comm::init()
{
    rce_uart_->set_baud(Uart::b9600);
    Uart::DataFormat df;
    df.db = Uart::d8;
    df.p = Uart::None;
    df.sb = Uart::One;
    rce_uart_->set_format(df);
    
    timer_->start();
    
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
    static uint8_t buf[] = "DATA!";

    uint32_t cnt = rce_uart_->read(command_buffer, COMMAND_BUFFER_SIZE);
    cur_str.resize(cnt);
    for (int i = 0; i < cnt; ++i)
        cur_str[i] = command_buffer[i];
    
    if (cnt > 0)
        dlog("Received string {}",cur_str);
    
    rce_uart_->write(command_buffer, cnt);

    timer_->update();

    if (timer_->elapsed() >= 2000)
    {
        timer_->start();
        rce_uart_->write(buf,5);
    }
}

std::string RCE_Serial_Comm::typestr()
{
    return TypeString();
}

std::string RCE_Serial_Comm::TypeString()
{
    return "RCE_Serial_Comm";
}
