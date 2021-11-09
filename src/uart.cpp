#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

#include "utility.h"
#include "uart.h"
#include "logger.h"

Uart::Uart(SerialPort uart_num):
	Threaded_Fd(),
	m_df(),
	m_baud(b115200)
{
	switch(uart_num)
	{
	  case (Uart1):
		  m_devpath = "/dev/ttyUSB0";
		  break;
	  case (Uart2):
		  m_devpath = "/dev/ttyUSB1";
		  break;
	}	
}

bool Uart::start()
{
    errno = 0;
	set_fd(open(m_devpath.c_str(), O_RDWR | O_NOCTTY | O_SYNC));
	if (m_fd < 0)
	{
        elog("Error opening {}: {}",m_devpath, strerror(errno));
		return false;
	}
    _set_attribs();
	return Threaded_Fd::start();
}

Uart::~Uart()
{}

const std::string & Uart::device_path()
{
	return m_devpath;
}

void Uart::set_baud(BaudRate baud)
{
	m_baud = baud;
	if (m_fd != -1)
		_set_attribs();
}

Uart::BaudRate Uart::baud()
{
	return m_baud;
}

void Uart::set_format(DataBits db, Parity p, StopBits sb)
{
	m_df.db = db;
	m_df.p = p;
	m_df.sb = sb;

	if (m_fd != -1)
		_set_attribs();
}

void Uart::set_format(const DataFormat & data_format)
{
	m_df = data_format;
	if (m_fd != -1)
		_set_attribs();	
}

const Uart::DataFormat & Uart::format()
{
	return m_df;
}

void Uart::_set_attribs()
{
	struct termios tty;
	memset(&tty, 0, sizeof(tty));
	if (tcgetattr(m_fd, &tty) != 0)
	{
		if (errno == EBADF)
            elog("Error getting termios - m_fd is not valid file descriptor");
		else if (errno == ENOTTY)
            elog("Error getting termios - the file associated with fildes is not a terminal");
		return;
	}
	
	tty.c_iflag = 0;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cflag = 0;
	
	tty.c_cflag |= (CLOCAL | CREAD | m_df.db | m_df.p | m_df.sb);

	// Set baud rates
	cfsetospeed (&tty, m_baud);
	cfsetispeed (&tty, m_baud);

	tty.c_cc[VMIN]  = 0;            // read doesn't block
	tty.c_cc[VTIME] = 1;            // 0.1 seconds read timeout


	if (tcsetattr(m_fd, TCSANOW, &tty) != 0)
        elog("Error - could not set tty");
}

int32_t Uart::_raw_read(uint8_t * buffer, uint32_t max_size)
{
    return ::read(m_fd, buffer, max_size);
}

int32_t Uart::_raw_write(uint8_t * buffer, uint32_t max_size)
{
	return ::write(m_fd, buffer, max_size);
}
