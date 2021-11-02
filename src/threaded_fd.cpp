#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "timer.h"
#include "callback.h"
#include "threaded_fd.h"
#include "utility.h"
#include "logger.h"

Threaded_Fd::Threaded_Fd(uint32_t readbuf_, uint32_t writebuf_)
    : m_fd(-1),
      m_read_rawindex(0),
      m_read_curindex(0),
      m_write_rawindex(0),
      m_write_curindex(0),
      write_buf_size_(writebuf_),
      read_buf_size_(readbuf_),
      m_current_wait_for_byte_count(0),
      m_wait_timer(new Timer()),
      m_thread(0)
{
    pthread_mutex_init(&m_send_lock, nullptr);
    pthread_mutex_init(&m_recv_lock, nullptr);
    pthread_mutex_init(&m_error_lock, nullptr);
    m_read_buffer = (uint8_t *)malloc(read_buf_size_);
    m_write_buffer = (WriteVal *)malloc(write_buf_size_ * sizeof(WriteVal));

    m_wait_timer->set_callback_delay(COMMAND_WAIT_DELAY);
    m_wait_timer->set_callback_mode(Timer::single_shot);

    using namespace std::placeholders;
    std::function<void(Timer *)> cb_func = std::bind(&Threaded_Fd::wait_callback_func, this, _1);
    m_wait_timer->set_callback(cb_func);
}

Threaded_Fd::~Threaded_Fd()
{
    pthread_mutex_destroy(&m_send_lock);
    pthread_mutex_destroy(&m_recv_lock);
    pthread_mutex_destroy(&m_error_lock);
    free(m_read_buffer);
    free(m_write_buffer);
    delete m_wait_timer;
    if (m_fd > 0)
        close(m_fd);
}

uint32_t Threaded_Fd::read(uint8_t * buffer, uint32_t max_size)
{
    uint32_t count = 0;
    pthread_mutex_lock(&m_recv_lock);
    while (m_read_curindex != m_read_rawindex)
    {
        buffer[count] = m_read_buffer[m_read_curindex];
        ++count;
        ++m_read_curindex;
        if (m_read_curindex == read_buf_size_)
            m_read_curindex = 0;
        if (count == max_size)
            break;
    }
    pthread_mutex_unlock(&m_recv_lock);
    return count;
}

uint32_t Threaded_Fd::write(const uint8_t * buffer, uint32_t size, int32_t response_size)
{
    int32_t resp = 0;
    pthread_mutex_lock(&m_send_lock);
    if (size > write_buf_size_)
        size = write_buf_size_;
    for (uint32_t i = 0; i < size; ++i)
    {
        if (i == size - 1)
            resp = response_size;
        m_write_buffer[m_write_curindex] = WriteVal(buffer[i], resp);
        ++m_write_curindex;
        if (m_write_curindex == m_write_rawindex)
            size = i;
        if (m_write_curindex == write_buf_size_)
            m_write_curindex = 0;
    }
    pthread_mutex_unlock(&m_send_lock);
    return size;
}

uint32_t Threaded_Fd::write(const char * buffer, int32_t response_size)
{
    size_t sz = strlen(buffer);
    return this->write((const uint8_t *)buffer, sz, response_size);
}

bool Threaded_Fd::running()
{
    bool ret = m_thread_running.test_and_set();
    if (!ret)
        m_thread_running.clear();
    return ret;
}

int32_t Threaded_Fd::fd()
{
    return m_fd;
}

Threaded_Fd::Error Threaded_Fd::error()
{
    pthread_mutex_lock(&m_error_lock);
    Error ret = m_err;
    pthread_mutex_unlock(&m_error_lock);
    return ret;
}

void Threaded_Fd::_setError(ErrorVal err_val, int32_t _errno)
{
    pthread_mutex_lock(&m_error_lock);
    m_err.err_val = err_val;
    m_err._errno = _errno;
    pthread_mutex_unlock(&m_error_lock);
}

bool Threaded_Fd::start()
{
    // lock mutex in case thread is already running
    if (running())
    {
        _setError(AlreadyRunning, 0);
        return false;
    }

    // Make sure to set before creating the thread!!
    m_thread_running.test_and_set();
    if (pthread_create(&m_thread, nullptr, Threaded_Fd::thread_exec, (void *)this) != 0)
    {
        _setError(ThreadCreation, errno);
        m_thread_running.clear();
        return false;
    }
    _setError(NoError, 0);
    return true;
}

bool Threaded_Fd::set_fd(int32_t fd_)
{
    if (running())
    {
        _setError(AlreadyRunning, 0);
        return false;
    }
    if (m_fd != -1)
        close(m_fd);
    m_fd = fd_;
    return true;
}

void Threaded_Fd::stop()
{
    ilog("Stopping thread for fd {}", m_fd);
    if (m_thread_running.test_and_set())
    {
        m_thread_running.clear();
    }
    else
    {
        ilog("Thread for fd {} already complete or never started (this is fine)", m_fd);
    }
    pthread_join(m_thread, nullptr);
    m_read_rawindex = 0;
    m_read_curindex = 0;
    m_write_rawindex = 0;
    m_write_curindex = 0;
    m_current_wait_for_byte_count = 0;
}

void Threaded_Fd::_do_read()
{
    int32_t cnt = _raw_read(m_tmp_read_buf, FD_TMP_BUFFER_SIZE);
    if (cnt < 0)
    {
        int32_t err_no = errno;
        if (err_no != EAGAIN && err_no != EWOULDBLOCK)
        {
            _setError(InvalidRead, err_no);
            m_thread_running.clear();
        }
    }
    pthread_mutex_lock(&m_recv_lock);
    for (int32_t i = 0; i < cnt; ++i)
    {
        m_read_buffer[m_read_rawindex] = m_tmp_read_buf[i];
        ++m_read_rawindex;

        if (m_current_wait_for_byte_count > 0)
        {
            --m_current_wait_for_byte_count;
            if (m_current_wait_for_byte_count == 0 && m_wait_timer->running())
                m_wait_timer->stop();
        }

        if (m_read_rawindex == read_buf_size_)
            m_read_rawindex = 0;

        // This check may go away eventually
        if (m_read_rawindex == m_read_curindex)
        {
            _setError(InvalidRead, 0);
            m_thread_running.clear();
        }
    }
    pthread_mutex_unlock(&m_recv_lock);
}

void Threaded_Fd::_do_write()
{
    int32_t tosend = 0;
    int32_t retval = 0;
    int32_t sent = 0;

    pthread_mutex_lock(&m_send_lock);
    while (m_write_rawindex != m_write_curindex)
    {
        WriteVal wv = m_write_buffer[m_write_rawindex];
        m_tmp_write_buf[tosend] = wv.byte;
        m_current_wait_for_byte_count = wv.response_size;

        ++m_write_rawindex;
        ++tosend;

        if (m_write_rawindex == write_buf_size_)
            m_write_rawindex = 0;

        if (m_current_wait_for_byte_count > 0)
        {
            m_wait_timer->start();
            break;
        }

        if (tosend == FD_TMP_BUFFER_SIZE)
            break;
    }
    pthread_mutex_unlock(&m_send_lock);

    while (sent != tosend)
    {
        retval = _raw_write(m_tmp_write_buf + sent, tosend - sent);
        if (retval == -1)
        {
            _setError(InvalidWrite, errno);
            m_thread_running.clear();
        }
        sent += retval;
    }
}

void Threaded_Fd::_exec()
{
    while (m_thread_running.test_and_set())
    {
        m_wait_timer->update();

        if (!m_wait_timer->running())
            _do_write();
        _do_read();
    }
    m_thread_running.clear();
}

void * Threaded_Fd::thread_exec(void * _this)
{
    Threaded_Fd * thfd = static_cast<Threaded_Fd *>(_this);
    ilog("Starting thread for fd {}", thfd->fd());
    thfd->_exec();
    return nullptr;
}

void Threaded_Fd::wait_callback_func(Timer * timer)
{
    timer->stop();
    _setError(Threaded_Fd::CommandNoResponse, 0);
}

std::string Threaded_Fd::error_string(const Threaded_Fd::Error & err)
{
    std::string ret;
    ret += "File descriptor error: " + std::string(strerror(err._errno));
    ret += "   Thread error: ";
    switch (err.err_val)
    {
    case (Threaded_Fd::NoError):
        ret += "None";
        break;
    case (Threaded_Fd::ConnectionClosed):
        ret += "Connection was closed";
        break;
    case (Threaded_Fd::DataOverwrite):
        ret += "Data was overwritten";
        break;
    case (Threaded_Fd::InvalidRead):
        ret += "Invalid read";
        break;
    case (Threaded_Fd::InvalidWrite):
        ret += "Invalid write";
        break;
    case (Threaded_Fd::ThreadCreation):
        ret += "Problem with thread creation";
        break;
    case (Threaded_Fd::OpenFileDescriptor):
        ret += "File descriptor already open";
        break;
    case (Threaded_Fd::Configuration):
        ret += "Error with configuration";
        break;
    case (Threaded_Fd::AlreadyRunning):
        ret += "Thread already running";
        break;
    case (Threaded_Fd::CommandNoResponse):
        ret += "No response to command";
        break;
    }
    return ret;
}
