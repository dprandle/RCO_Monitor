#pragma once

#include "threaded_fd.h"

class Socket : public Threaded_Fd
{
  public:

    Socket(int32_t socket_fd);
	
	~Socket();

  private:

	int32_t _raw_read(uint8_t * buffer, uint32_t max_size);
	int32_t _raw_write(uint8_t * buffer, uint32_t max_size);

};
