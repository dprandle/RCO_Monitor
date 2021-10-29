#pragma once

#include "threaded_fd.h"

class Socket : public Threaded_Fd
{
  public:

    Socket();
	~Socket();

    int connect(const std::string & ip_address, int16_t port, int8_t timeout_secs);

    std::string & get_ip();

    int16_t get_port();

  private:
	int32_t _raw_read(uint8_t * buffer, uint32_t max_size);
	int32_t _raw_write(uint8_t * buffer, uint32_t max_size);

    std::string _ip;
    int16_t _port;
};
