#pragma once

#include <pthread.h>
#include <vector>
#include <string>
#include <atomic>

#include "callback.h"

#define DEFAULT_FD_WRITE_BUFFER_SIZE 5120
#define DEFAULT_FD_READ_BUFFER_SIZE 600000
#define FD_TMP_BUFFER_SIZE 1024
#define COMMAND_WAIT_DELAY 1

class Timer;

class Threaded_Fd
{
  public:

	enum ErrorVal
	{
		NoError,
        ConnectionClosed,
		DataOverwrite,
		InvalidRead,
		InvalidWrite,
		ThreadCreation,
		OpenFileDescriptor,
		Configuration,
		AlreadyRunning,
		CommandNoResponse
	};

	struct Error
	{
		Error():
			err_val(NoError),
			_errno(0)
		{}
		
		ErrorVal err_val;
		int32_t _errno;
	};

	struct WriteVal
	{
		WriteVal(uint8_t byte_=0x00, int32_t response_size_= 0):
			byte(byte_),
			response_size(response_size_)
		{}
		uint8_t byte;
        int32_t response_size;
	};
	
	Threaded_Fd(
		uint32_t readbuf_size = DEFAULT_FD_READ_BUFFER_SIZE,
		uint32_t writebuf_size = DEFAULT_FD_WRITE_BUFFER_SIZE);
	
    virtual ~Threaded_Fd();

    virtual uint32_t read(uint8_t * buffer, uint32_t max_size);

    virtual uint32_t write(const uint8_t * buffer, uint32_t size, int32_t response_size = 0);

    virtual uint32_t write(const char * buffer, int32_t response_size = 0);

	virtual Error error();

	bool running();

	virtual bool start();

	int32_t fd();

	bool set_fd(int32_t fd_);
	
    void stop();

    static std::string error_string(const Error & err);
	
  protected:

	friend struct command_wait_callback;
	
	virtual int32_t _raw_read(uint8_t * buffer, uint32_t max_size) = 0;
	virtual int32_t _raw_write(uint8_t * buffer, uint32_t max_size) = 0;

	void wait_callback_func(Timer * timer);

	virtual void _do_read();
	virtual void _do_write();
	
	virtual void _exec();
	void _setError(ErrorVal err_val, int32_t _errno);

	static void * thread_exec(void *);
	
    std::atomic_int_fast32_t m_fd;
	uint32_t m_read_rawindex;
	uint32_t m_read_curindex;
	uint32_t m_write_rawindex;
	uint32_t m_write_curindex;
	
	WriteVal * m_write_buffer;
	uint8_t * m_read_buffer;
    uint32_t write_buf_size_;
    uint32_t read_buf_size_;


	Error m_err;

	uint32_t m_current_wait_for_byte_count;
	Timer * m_wait_timer;
	
	pthread_mutex_t m_send_lock;
	pthread_mutex_t m_recv_lock;
	pthread_mutex_t m_error_lock;


    uint8_t m_tmp_read_buf[FD_TMP_BUFFER_SIZE];
    uint8_t m_tmp_write_buf[FD_TMP_BUFFER_SIZE];

    std::atomic_flag m_thread_running = ATOMIC_FLAG_INIT;
	
	pthread_t m_thread;
};

struct command_wait_callback : public Wait_Ready_Callback
{
	command_wait_callback(Threaded_Fd * _handle):
		handle(_handle)
	{}
	
	void exec()
	{
		Wait_Ready_Callback::exec();
		handle->_setError(Threaded_Fd::CommandNoResponse, 0);
	}
	Threaded_Fd * handle;
};
