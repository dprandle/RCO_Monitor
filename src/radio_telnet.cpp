#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "logger.h"
#include "socket.h"
#include "radio_telnet.h"

Radio_Telnet::Radio_Telnet()
{}

Radio_Telnet::~Radio_Telnet()
{}

void Radio_Telnet::init()
{
    sockaddr_in server_addr;
    int32_t sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        ilog("Failed to create socket! Comm with radio won't work");
        return;
    }

    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("192.168.102.9");
    server_addr.sin_port = htons(8081);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0)
    {
        ilog("Failed to connection to server");
        return;
    }

    rad_rx = new Socket(sockfd);
    if (!rad_rx->start())
    {
        ilog("Could not start socket threaded fd: {}", Threaded_Fd::error_string(rad_rx->error()));
        delete rad_rx;
        rad_rx = nullptr;
    }
}

void Radio_Telnet::release()
{
    Subsystem::release();
    delete rad_rx;
}

void Radio_Telnet::update()
{
    static bool sendonce = true;
    if (sendonce)
    {
        rad_rx->write("MEAS?\n");
        sendonce = false;
    }

    static uint8_t tmp_buf[96];
    uint32_t cnt = rad_rx->read(tmp_buf, 96);
    for (int i = 0; i < cnt; ++i)
    {
        printf("%c",tmp_buf[i]);
    }
}

const char * Radio_Telnet::typestr()
{
    return TypeString();
}

const char * Radio_Telnet::TypeString()
{
    return "Radio_Telnet";
}
