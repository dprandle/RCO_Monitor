#pragma once

#include "threaded_fd.h"

struct Timeout_Interval
{
    uint16_t secs;
    uint16_t usecs;
};

class Socket : public Threaded_Fd
{
  public:

    Socket();
	~Socket();

    int connect(const std::string & ip_address, int16_t port, const Timeout_Interval & timeout);

    std::string & get_ip();

    int16_t get_port();

  private:
	int32_t _raw_read(uint8_t * buffer, uint32_t max_size);
	int32_t _raw_write(uint8_t * buffer, uint32_t max_size);

    std::string _ip;
    int16_t _port;
};
