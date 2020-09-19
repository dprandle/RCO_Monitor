#include "rce_serial_comm.h"
#include "uart.h"
#include "utility.h"
#include "logger.h"
#include "timer.h"
#include <string>

bool Firmware_Update::process(uint8_t * buffer, uint32_t size)
{
    static int mult = 1;
    static std::string prev_str = "0%";

    for (int i = 0; i < size; ++i)
    {
        if (current_ind < FIRMWARE_HEADER_SIZE)
        {
            hdr.data[current_ind] = buffer[i];
        }
        else
        {
            int payload_ind = current_ind - FIRMWARE_HEADER_SIZE;
            payload[payload_ind] = buffer[i];
            if (payload_ind > (mult * hdr.byte_size / 1000))
            {
                ++mult;
                std::string bsstr;
                for (int i = 0; i < prev_str.size(); ++i)
                    bsstr += "\b";
                prev_str = std::to_string(double(mult)*0.1) + "%";
                std::string str(bsstr + prev_str);
                rce_uart->write(str.c_str());
            }
        }
        ++current_ind;

        // This means we need to resize the payload!
        if (current_ind == FIRMWARE_HEADER_SIZE)
        {
            payload.resize(hdr.byte_size);
            std::string str("Received byte size of " + std::to_string(hdr.byte_size) + "\r");
            std::string fmw("Firmware - v" + std::to_string(hdr.v_major) + "." + std::to_string(hdr.v_minor) + "." + std::to_string(hdr.v_patch) + "\r");
            std::string progress("Progress: " + prev_str);
            rce_uart->write(str.c_str());
            rce_uart->write(fmw.c_str());
            rce_uart->write(progress.c_str());
        }
        else if (current_ind == (FIRMWARE_HEADER_SIZE + hdr.byte_size))
        {
            current_ind = 0;
            mult = 1;
            prev_str = "0%";
            return true;
        }
    }
    return false;
}

RCE_Serial_Comm::RCE_Serial_Comm() : rce_uart_(new Uart(Uart::Uart1))
{
    command_buffer.reserve(COMMAND_BUFFER_MAX_SIZE);
}

RCE_Serial_Comm::~RCE_Serial_Comm()
{
    delete rce_uart_;
}

void RCE_Serial_Comm::init()
{
    // Create the default commands
    add_command<Firmware_Update>("FWU");

    rce_uart_->set_baud(Uart::b9600);
    Uart::DataFormat df;
    df.db = Uart::d8;
    df.p = Uart::None;
    df.sb = Uart::One;
    rce_uart_->set_format(df);

    rce_uart_->start();
    rce_uart_->write("Starting RCO_Monitor\r");
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

    if (current_command.empty())
    {
        for (int i = 0; i < cnt; ++i)
        {
            if (tmp_buf[i] == '\r')
            {
                check_buffer_for_command_();
                command_buffer.resize(0);
                continue;
            }

            if (command_buffer.size() == COMMAND_BUFFER_MAX_SIZE)
            {
                wlog("Reached max size of command buffer without command (buffer:{}) - resetting", command_buffer);
                rce_uart_->write("\r");
                command_buffer.resize(0);
            }

            command_buffer.push_back(tmp_buf[i]);
        }
    }
    else
    {
        Command_Handler * handler = get_command(current_command);
        if (handler)
        {
            if (handler->process(tmp_buf, cnt))
            {
                std::string msg = "\rCommand: " + current_command + " complete\r\n";
                rce_uart_->write(msg.c_str());
                current_command.clear();
            }
        }
        else
        {
            current_command.clear();
        }
    }
}

Command_Handler * RCE_Serial_Comm::get_command(const std::string & command)
{
    auto fiter = command_handlers_.find(command);
    if (fiter != command_handlers_.end())
        return fiter->second;
    return nullptr;
}

bool RCE_Serial_Comm::remove_command(const std::string & command)
{
    auto fiter = command_handlers_.find(command);
    if (fiter != command_handlers_.end())
    {
        delete fiter->second;
        command_handlers_.erase(fiter);
        return true;
    }
    return false;
}

void RCE_Serial_Comm::check_buffer_for_command_()
{
    Command_Handler * handler = get_command(command_buffer);
    if (handler)
    {
        current_command = command_buffer;
        std::string inv("Executing command: " + command_buffer + "...\r");
        rce_uart_->write(inv.c_str());
    }
    else
    {
        std::string inv("Invalid command entered: " + command_buffer + "\r");
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
