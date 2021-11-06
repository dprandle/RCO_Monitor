#include <cstring>
#include <sys/stat.h>

#include "main_control.h"
#include "rce_serial_comm.h"
#include "shared_structs.h"
#include "uart.h"
#include "logger.h"
#include "timer.h"

bool Firmware_Update::process(uint8_t * buffer, uint32_t size)
{
    static uint8_t mult = 0;
    static uint8_t to_send = 0;
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
            if (payload_ind > (mult * hdr.byte_size / 100))
            {
                // Never send a newline or carraige return character - those indicate end of transmission
                to_send = mult + 32;
                ilog("Downloading ({}%) - actual sent byte {}",mult,to_send);
                rce_uart->write(&to_send,1);
                ++mult;
            }
        }
        ++current_ind;

        // This means we need to resize the payload!
        if (current_ind == FIRMWARE_HEADER_SIZE)
        {
            payload = (uint8_t *)malloc(hdr.byte_size);
            memset(payload,0,hdr.byte_size);
            //rce_uart->write("Received firmware header\r");
            ilog("Received header for firmware {} - resize payload to {} bytes",parse_firmware_header(hdr), hdr.byte_size);
            if (hdr.byte_size == 0)
            {
                wlog("Byte size for firmware sent was 0 - likely error - exiting firmware download mode");
                return true;
            }
        }
        else if (current_ind == (FIRMWARE_HEADER_SIZE + hdr.byte_size))
        {
            char * path = parse_firmware_header(hdr);
            util::save_data_to_file(payload, hdr.byte_size, path, S_IRWXU);
            rce_uart->write("Successfully uploaded firmware to ");
            rce_uart->write(path);

            current_ind = 0;
            mult = 0;
            hdr = Firmware_Header();
            free(payload);
            payload = nullptr;
            return true;
        }
    }
    return false;
}

bool Reboot_Updated_Firmware::process(uint8_t * buffer, uint32_t size)
{
    for (int i = 0; i < size; ++i)
    {
        hdr.data[current_ind] = buffer[i];
        ++current_ind;
        if (current_ind == FIRMWARE_HEADER_SIZE)
        {
            current_ind = 0;
            const char * params[2] = {"-port=10000",nullptr};
            char * fwh_str = parse_firmware_header(hdr);
            ilog("Received reboot command - rebooting in to {}",fwh_str);
            edm.restart_updated(parse_firmware_header(hdr), params);
            return true;
        }
    }
    return false;
}

bool Get_Firmware_Versions::process(uint8_t * cmd_buffer, uint32_t size)
{   
    char ** buffer = nullptr;
    uint8_t cnt = util::filenames_in_dir("/home/ubuntu/bin",buffer);
    rce_uart->write(&cnt,1);
    ilog("Found {} versions of firmware...",cnt);
    for (int i = 0; i < cnt; ++i)
    {
        Firmware_Header hdr;
        parse_filename(buffer[i],hdr);
        if (hdr.v_major == CURRENT_VERSION_MAJOR && hdr.v_minor == CURRENT_VERSION_MINOR && hdr.v_patch == CURRENT_VERSION_PATCH)
            hdr.byte_size = 1;
        ilog("Sending back firmware {}",buffer[i]);
        rce_uart->write(hdr.data,FIRMWARE_HEADER_SIZE);
    }
    free(buffer);
    return true;
}

RCE_Serial_Comm::RCE_Serial_Comm() : reset_timer_(new Timer), rce_uart_(new Uart(Uart::Uart1))
{
    memset(current_command, 0, COMMAND_BUFFER_MAX_SIZE);
    memset(command_buffer, 0, COMMAND_BUFFER_MAX_SIZE);
    memset(command_handlers_, 0, MAX_COMMAND_COUNT);
}

RCE_Serial_Comm::~RCE_Serial_Comm()
{
    for (int i = 0; i < MAX_COMMAND_COUNT; ++i)
    {
        if (command_handlers_[i])
            delete command_handlers_[i];
        command_handlers_[i] = nullptr;
    }
    delete reset_timer_;
    delete rce_uart_;
}

void RCE_Serial_Comm::init(Config_File * config)
{
    Subsystem::init(config);
    // Create the default commands
    add_command<Reboot_Updated_Firmware>("RBUFW");
    add_command<Firmware_Update>("FWU");
    add_command<Get_Firmware_Versions>("GFV");

    rce_uart_->set_baud(Uart::b9600);
    Uart::DataFormat df;
    df.db = Uart::d8;
    df.p = Uart::None;
    df.sb = Uart::One;
    rce_uart_->set_format(df);

    rce_uart_->start();
    rce_uart_->write("Starting RCO Monitor\r");
}

void RCE_Serial_Comm::release()
{
    Subsystem::release();
    rce_uart_->stop();
}

void RCE_Serial_Comm::update()
{
    static uint8_t tmp_buf[96];
    uint32_t cnt = rce_uart_->read(tmp_buf, TMP_BUF_SIZE);

    if (util::buf_len(current_command, COMMAND_BUFFER_MAX_SIZE) == 0)
    {
        for (int i = 0; i < cnt; ++i)
        {
            if (tmp_buf[i] == '\r')
            {
                check_buffer_for_command_();
                memset(command_buffer, 0, COMMAND_BUFFER_MAX_SIZE);
                continue;
            }

            uint32_t cur_ind = util::buf_len(command_buffer, COMMAND_BUFFER_MAX_SIZE);
            if (cur_ind == COMMAND_BUFFER_MAX_SIZE)
            {
                wlog("Reached max size of command buffer without command (buffer:{}) - resetting", command_buffer);
                rce_uart_->write("\r");
                memset(command_buffer, 0, COMMAND_BUFFER_MAX_SIZE);
            }
            command_buffer[cur_ind] = tmp_buf[i];
        }
    }
    else
    {
        Command_Handler * handler = get_command(current_command);
        reset_timer_->update();
        if (cnt >= 1)
            reset_timer_->start();
        
        if (handler)
        {
            if (handler->process(tmp_buf, cnt))
            {
                rce_uart_->write("\rCommand: ");
                rce_uart_->write(current_command);
                rce_uart_->write(" complete\r\n");
                ilog("Command {} complete",current_command);
                memset(current_command, 0, COMMAND_BUFFER_MAX_SIZE);
                reset_timer_->stop();
            }
            else if (reset_timer_->elapsed() > MAX_TIMEOUT_MS)
            {
                ilog("Connection for command {} timed out - resetting", current_command);
                reset_timer_->stop();
                memset(current_command, 0, COMMAND_BUFFER_MAX_SIZE);
            }
        }
        else
        {
            memset(current_command, 0, COMMAND_BUFFER_MAX_SIZE);
        }
    }
}

Command_Handler * RCE_Serial_Comm::get_command(const char * command)
{
    uint32_t buf_len = util::buf_len(command_handlers_, MAX_COMMAND_COUNT);
    for (uint32_t i = 0; i < buf_len; ++i)
    {
        if (strcmp(command_handlers_[i]->command, command) == 0)
            return command_handlers_[i];
    }
    return nullptr;
}


void RCE_Serial_Comm::check_buffer_for_command_()
{
    Command_Handler * handler = get_command(command_buffer);
    if (handler)
    {
        strcpy(current_command, command_buffer);
        rce_uart_->write("Executing command: ");
        rce_uart_->write(current_command);
        rce_uart_->write("...\r");
        ilog("Recieved command: {} - executing...",current_command);
        reset_timer_->start();
    }
    else
    {
        rce_uart_->write("Invalid command entered: ");
        rce_uart_->write(command_buffer);
        rce_uart_->write("\r");
        wlog("Invalid command recieved: {}",command_buffer);
    }
}

const char * RCE_Serial_Comm::typestr()
{
    return TypeString();
}

const char * RCE_Serial_Comm::TypeString()
{
    return "RCE_Serial_Comm";
}
