#include "rce_serial_comm.h"
#include "uart.h"
#include "utility.h"
#include "logger.h"
#include "timer.h"

RCE_Serial_Comm::RCE_Serial_Comm() : rce_uart_(new Uart(Uart::Uart1)), timer_(new Timer())
{
    command_buffer.reserve(COMMAND_BUFFER_MAX_SIZE);
}

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
    static uint8_t tmp_buf[96];
    uint32_t cnt = rce_uart_->read(tmp_buf, TMP_BUF_SIZE);

    for (int i = 0; i < cnt; ++i)
    {
        command_buffer.push_back(tmp_buf[i]);
        if (command_buffer.size() > COMMAND_BUFFER_MAX_SIZE)
        {
            wlog("Reached max size of command buffer without command (buffer:{}) - resetting",command_buffer);
            command_buffer.resize(0);
        }
    }
    check_buffer_for_command_();
}

void RCE_Serial_Comm::check_buffer_for_command_()
{
    if (command_buffer.find(update_firmware_id) != std::string::npos)
    {
        rce_uart_->write("Found Firmware Command!",23);
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
