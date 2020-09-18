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
    rce_uart_->write("\n\rStarting RCO_Monitor\n\r>>> ");
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
        if (tmp_buf[i] == 127)
        {
            rce_uart_->write("\b \b");
            command_buffer.pop_back();
            continue;
        }

        if (tmp_buf[i] == '\r')
        {
            rce_uart_->write("\n\r");
            check_buffer_for_command_();
            rce_uart_->write("\n\r>>> ");
            command_buffer.resize(0);
            continue;
        }


        if (command_buffer.size() == COMMAND_BUFFER_MAX_SIZE)
        {
            wlog("Reached max size of command buffer without command (buffer:{}) - resetting",command_buffer);
            rce_uart_->write("\n\r>>> ");
            command_buffer.resize(0);
        }

        // Echo letters back
        rce_uart_->write(&tmp_buf[i],1);

        command_buffer.push_back(tmp_buf[i]);
    }
}

void RCE_Serial_Comm::check_buffer_for_command_()
{
    if (command_buffer == update_firmware_id)
    {
        rce_uart_->write("Found Firmware Command!");
    }
    else
    {
        std::string inv("Invalid command entered: " + command_buffer);
        rce_uart_->write(inv.c_str());
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
