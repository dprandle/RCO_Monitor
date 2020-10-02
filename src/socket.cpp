#include <socket.h>
#include <unistd.h>
#include <sys/socket.h>

Socket::Socket(int32_t socket_fd):
	Threaded_Fd()
{
	set_fd(socket_fd);
}
	
Socket::~Socket()
{}

int32_t Socket::_raw_read(uint8_t * buffer, uint32_t max_size)
{
	int32_t cnt = recv(m_fd, buffer, max_size, MSG_DONTWAIT);
	if (cnt == 0)
	{
		int32_t err_no = errno;
		_setError(ConnectionClosed, err_no);
		stop();
	}
	return cnt;
}

int32_t Socket::_raw_write(uint8_t * buffer, uint32_t max_size)
{
	return ::write(m_fd, buffer, max_size);
}
