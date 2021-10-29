#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "socket.h"
#include "logger.h"

Socket::Socket() : Threaded_Fd(), _port(0)
{
    int32_t socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        ilog("Failed to create socket");
        return;
    }
    set_fd(socket_fd);
}

int Socket::connect(const std::string & ip_address, int16_t port, int8_t timeout_secs)
{
    _ip = ip_address;
    _port = port;
    sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
    server_addr.sin_port = htons(port);

    struct timeval timeout;
    timeout.tv_sec = timeout_secs; // after 2 seconds connect() will timeout
    timeout.tv_usec = 0;
    setsockopt(fd(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    return ::connect(fd(), (struct sockaddr *)&server_addr, sizeof(server_addr));
}

Socket::~Socket()
{
    stop();
}

std::string & Socket::get_ip()
{
    return _ip;
}

int16_t Socket::get_port()
{
    return _port;
}

int32_t Socket::_raw_read(uint8_t * buffer, uint32_t max_size)
{
    int32_t cnt = recv(m_fd, buffer, max_size, MSG_DONTWAIT);
    if (cnt == 0)
    {
        int32_t err_no = errno;
        _setError(ConnectionClosed, err_no);
        m_thread_running.clear();
    }
    return cnt;
}

int32_t Socket::_raw_write(uint8_t * buffer, uint32_t max_size)
{
    return ::write(m_fd, buffer, max_size);
}
