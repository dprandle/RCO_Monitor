#include <stdlib.h>

#include "logger.h"
#include "socket.h"
#include "radio_telnet.h"

const std::string CMD_FREQ = "FREQ?";
const std::string CMD_ID = "ID?";
const std::string CMD_MEAS = "MEAS?";
const std::string CMD_TYPE = "TYPE?";
const std::string CMD_RSTAT = "RSTAT?";

Radio_Telnet::Radio_Telnet(uint8_t ip_lower_bound, uint8_t ip_upper_bound)
    : _ip_lb(ip_lower_bound), _ip_ub(ip_upper_bound), _logging(false), commands{CMD_FREQ, CMD_ID, CMD_MEAS, CMD_TYPE, CMD_RSTAT}
{}

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

        if (rad.sk->connect(ip, 8081, 1) != 0)
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
        _radios.push_back(rad);
    }
}

void Radio_Telnet::release()
{
    Subsystem::release();
    for (int i = 0; i < _radios.size(); ++i)
    {

        delete _radios[i].sk;
    }
    _radios.clear();
}

void Radio_Telnet::update()
{
    _update_closed();
    // Get updates for all the radios if interval has passed

    // static bool sendonce = true;
    // if (sendonce)
    // {
    //     rad_rx->write("MEAS?\n");
    //     sendonce = false;
    // }

    // static uint8_t tmp_buf[96];
    // uint32_t cnt = rad_rx->read(tmp_buf, 96);
    // for (int i = 0; i < cnt; ++i)
    // {
    //     printf("%c",tmp_buf[i]);
    // }
}

void Radio_Telnet::_update_closed()
{
    auto iter = _radios.begin();
    while (iter != _radios.end())
    {
        if (iter->sk->error().err_val != Threaded_Fd::NoError)
        {
            ilog("Closing connection to {}:{} on fd {} due to error: {}", iter->sk->get_ip(), iter->sk->get_port(), iter->sk->fd(), Threaded_Fd::error_string(iter->sk->error()));
            delete iter->sk;
            iter = _radios.erase(iter);
        }
        else
        {
            ++iter;
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
