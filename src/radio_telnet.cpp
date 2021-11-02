#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>

#include "logger.h"
#include "radio_telnet.h"

namespace cmd
{
namespace str
{
const std::string FREQ = "FREQ?\n";
const std::string ID = "ID?\n";
const std::string MEAS = "MEAS?\n";
const std::string RSTAT = "RSTAT?\n";
} // namespace str
namespace ind
{
const uint8_t FREQ = 0;
const uint8_t ID = 1;
const uint8_t MEAS = 2;
const uint8_t RSTAT = 3;
} // namespace ind
} // namespace cmd

static int resp_len = 0;

Radio_Telnet::Radio_Telnet(uint8_t ip_lower_bound, uint8_t ip_upper_bound)
    : _ip_lb(ip_lower_bound), _ip_ub(ip_upper_bound), _logging(false), commands{cmd::str::FREQ, cmd::str::ID, cmd::str::MEAS, cmd::str::RSTAT}
{
    resp_len = strlen(RESPONSE_COMPLETE_STR);
}

Radio_Telnet::~Radio_Telnet()
{}

void Radio_Telnet::init()
{
    Subsystem::init();
    int8_t size = (_ip_ub - _ip_lb + 1);
    int64_t arg = 0;

    for (int8_t i = 0; i < size; ++i)
    {
        std::string ip_last_octet = std::to_string(_ip_lb + i);
        std::string ip = "192.168.102." + ip_last_octet;

        CM300_Radio rad;
        rad.sk = new Socket();

        if (rad.sk->fd() == -1)
        {
            ilog("Failed to create socket for {} - error: ", ip, strerror(errno));
            delete rad.sk;
            continue;
        }
        ilog("Attempting to connect to radio at {} on socket fd {}", ip, rad.sk->fd());

        if (rad.sk->connect(ip, 8081, _conn_timeout) != 0)
        {
            ilog("Connection timeout to {} - no radio found", ip, strerror(errno));
            delete rad.sk;
            continue;
        }

        if (!rad.sk->start())
        {
            ilog("Could not start socket for {} on threaded fd: {}", ip, Threaded_Fd::error_string(rad.sk->error()));
            delete rad.sk;
        }
        ilog("Opened connection to radio at {} on socket fd {}", ip, rad.sk->fd());
        rad.cur_cmd = cmd::ind::FREQ;
        _radios.push_back(rad);
    }
}

void Radio_Telnet::set_max_retry_count(uint8_t max_retry_count)
{
    _max_retry_count = max_retry_count;
}

uint8_t Radio_Telnet::get_max_retry_count() const
{
    return _max_retry_count;
}

void Radio_Telnet::set_connection_timeout(const Timeout_Interval & wait_for_connection_for)
{
    _conn_timeout = wait_for_connection_for;
}

const Timeout_Interval & Radio_Telnet::get_connection_timeout() const
{
    return _conn_timeout;
}

void Radio_Telnet::release()
{
    Subsystem::release();
    for (int i = 0; i < _radios.size(); ++i)
        delete _radios[i].sk;
    _radios.clear();
}

void Radio_Telnet::update()
{
    auto iter = _radios.begin();
    while (iter != _radios.end())
    {
        _update(&(*iter));
        _update_closed(&(*iter));
        ++iter;
    }
}

void Radio_Telnet::_update(CM300_Radio * radio)
{
    if (!radio->sk)
        return;

    uint32_t cnt = radio->sk->read(radio->response_buffer + radio->buffer_offset, BUFFER_SIZE - radio->buffer_offset);
    radio->buffer_offset += cnt; // buffer offset holds the new size

    if (radio->buffer_offset >= resp_len)
    {
        if (strncmp((char *)radio->response_buffer + (radio->buffer_offset - cnt), RESPONSE_COMPLETE_STR, resp_len))
        {
            ilog("Received complete packet for {} at {} (buffer size: {})!", radio->cur_cmd, radio->sk->get_ip(), radio->buffer_offset);
            radio->response_buffer[radio->buffer_offset] = '\0';
            radio->buffer_offset = 0;
            radio->prev_cmd = radio->cur_cmd;
            radio->cur_cmd = -1;
            _parse_response_to_radio_data(radio);
            bzero(radio->response_buffer, resp_len);
        }
    }

    if (radio->cur_cmd == INVALID_VALUE)
    {
        radio->cur_cmd = radio->prev_cmd + 1;
        if (radio->cur_cmd > cmd::ind::RSTAT)
            radio->cur_cmd = cmd::ind::FREQ;
        ilog("Done with prev command: {} - sending next command: {}", radio->prev_cmd, radio->cur_cmd);
        radio->sk->write(commands[radio->cur_cmd].c_str());
        //sleep(1);
    }
}

void Radio_Telnet::_update_closed(CM300_Radio * radio)
{
    if (radio->sk && radio->sk->error().err_val != Threaded_Fd::NoError)
    {
        ilog("Closing connection to {}:{} on fd {} due to error: {}",
             radio->sk->get_ip(),
             radio->sk->get_port(),
             radio->sk->fd(),
             Threaded_Fd::error_string(radio->sk->error()));
        radio->sk->stop();
        if (radio->retry_count < _max_retry_count)
        {
            ++radio->retry_count;
            ilog("Attempting to reconnect to radio at {} on socket fd {}", radio->sk->get_ip(), radio->sk->fd());

            if (radio->sk->connect(radio->sk->get_ip(), radio->sk->get_port(), _conn_timeout) == 0)
            {
                if (radio->sk->start())
                {
                    ilog("Opened connection to radio at {} on socket fd {}", radio->sk->get_ip(), radio->sk->fd());
                }
                else
                {
                    ilog("Could not start socket for {} on threaded fd: {}", radio->sk->get_ip(), Threaded_Fd::error_string(radio->sk->error()));
                }
            }
            else
            {
                ilog("Connection timeout to {}", radio->sk->get_ip(), strerror(errno));
            }
        }
        else
        {
            ilog("Reached max retry count for {} - giving up. Bye bye to socket on fd {}.", radio->sk->get_ip(), radio->sk->fd());
            delete radio->sk;
            radio->sk = nullptr;
        }
    }
}

void Radio_Telnet::_parse_response_to_radio_data(CM300_Radio * radio)
{
    std::string curline;

    for (int i = 0; i < radio->buffer_offset; ++i)
    {
        char c = radio->response_buffer[i];
        radio->response_buffer[i] = 0;
        if (c == '\n')
        {
            if (curline.find(':') != std::string::npos)
            {
                ilog("Received line: {}", curline);
            }
            curline.clear();
        }
        else if (c > ' ')
        {
            curline.push_back(c);
        }
    }
}

void Radio_Telnet::enable_logging(bool enable)
{
    _logging = enable;
}

const char * Radio_Telnet::typestr()
{
    return TypeString();
}

const char * Radio_Telnet::TypeString()
{
    return "Radio_Telnet";
}
