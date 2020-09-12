#include <edgpio.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <edutility.h>
#include <edtimer.h>
#include <edlogger.h>

edgpio::edgpio(int pin)
    : m_isr_func(nullptr), m_err(), m_pin(pin), m_thread_running(), m_isr_thread(), m_isr(false)
{
    char buffer[8];
    int pind = m_pin;
    int n = sprintf(buffer, "%d", pind);
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1)
    {
        m_err.errno_code = errno;
        m_err.gp_code |= gpio_export_error;
    }
    else
    {
        write(fd, buffer, n);
        close(fd);
    }
}

edgpio::~edgpio()
{
    // Close the isr thread if needed
    if (m_thread_running.test_and_set())
    {
        m_thread_running.clear();
        m_err.errno_code = pthread_join(m_isr_thread, nullptr);
        if (m_err.errno_code)
        {
            // error joining - set error code and return
            m_err.gp_code |= gpio_thread_join_error;
        }
    }

    char buffer[8];
    int pin = m_pin;
    int n = sprintf(buffer, "%d", pin);
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1)
    {
        m_err.errno_code = errno;
        m_err.gp_code |= gpio_unexport_error;
    }
    else
    {
        write(fd, buffer, n);
        close(fd);
    }
}

gpio_error_state edgpio::get_and_clear_error()
{
    gpio_error_state tmp = m_err;
    m_err.errno_code = 0;
    m_err.gp_code = gpio_no_error;
    return tmp;
}

int edgpio::set_direction(gpio_dir dir)
{
    char buf[FILE_BUF_SZ];
    int pin = m_pin;
    sprintf(buf, "/sys/class/gpio/gpio%d/direction", pin);
    int fd = open(buf, O_WRONLY);
    if (fd == -1)
    {
        m_err.errno_code = errno;
        m_err.gp_code |= gpio_direction_error;
        return -1;
    }
    else
    {
        int tmp = 0;
        switch (dir)
        {
        case (gpio_dir_out):
            tmp = write(fd, "out", 3);
            break;
        case (gpio_dir_in):
            tmp = write(fd, "in", 2);
            break;
        case (gpio_dir_out_high):
            tmp = write(fd, "high", 4);
            break;
        case (gpio_dir_out_low):
            tmp = write(fd, "low", 3);
            break;
        }
        close(fd);
        if (tmp == -1)
        {
            m_err.errno_code = errno;
            m_err.gp_code |= gpio_direction_error;
            return -1;
        }
    }
    return 0;
}

int edgpio::direction()
{
    char buf[FILE_BUF_SZ];
    char r_buf[5];
    int pin = m_pin;
    sprintf(buf, "/sys/class/gpio/gpio%d/direction", pin);
    int fd = open(buf, O_RDONLY);
    if (fd == -1)
    {
        m_err.errno_code = errno;
        m_err.gp_code |= gpio_direction_error;
    }
    else
    {
        int bytes = read(fd, r_buf, 5);
        close(fd);
        if (bytes == -1)
        {
            m_err.errno_code = errno;
            m_err.gp_code |= gpio_direction_error;
        }
        if (strncmp(r_buf, "in", 2) == 0) // for clarity == 0
            return gpio_dir_in;
        if (strncmp(r_buf, "out", 3) == 0) // for clarity == 0
            return gpio_dir_out;
    }
    return -1;
}

int edgpio::set_isr(gpio_isr_edge edge, std::function<void()> func)
{
    // Close the isr thread if needed - no matter what after this the thread should be dead
    if (m_thread_running.test_and_set())
    {
        m_thread_running.clear();
        m_err.errno_code = pthread_join(m_isr_thread, nullptr);
        if (m_err.errno_code)
        {
            // error joining - set error code and return
            m_err.gp_code |= gpio_thread_join_error;
            return -1;
        }
    }
    m_thread_running.clear(); // need to set false now because of previous if test

    if (edge == gpio_edge_none)
    {
        m_isr_func = func;
        return 0;
    }

    if (func == nullptr) // no isr if func is null
        edge = gpio_edge_none;

    char buf[FILE_BUF_SZ];
    int pin = m_pin;
    sprintf(buf, "/sys/class/gpio/gpio%d/edge", pin);
    int fd = open(buf, O_WRONLY);
    if (fd == -1)
    {
        m_err.errno_code = errno;
        m_err.gp_code |= gpio_file_open_error;
        return -1;
    }
    else
    {
        int tmp = 0;
        m_isr_func = func;
        switch (edge)
        {
        case (gpio_edge_none): // basically disable isr and don't start the thread!
            tmp = write(fd, "none", 4);
            if (tmp == -1)
            {
                m_err.errno_code = errno;
                m_err.gp_code = gpio_edge_error;
                close(fd);
                return -1;
            }
            close(fd);
            return 0;
        case (gpio_edge_both): // trigger isr for both edges
            tmp = write(fd, "both", 4);
            break;
        case (gpio_edge_rising): // trigger isr for rising edges only
            tmp = write(fd, "rising", 6);
            break;
        case (gpio_edge_falling): // trigger isr for falling edges only
            tmp = write(fd, "falling", 7);
            break;
        }
        close(fd);
        if (tmp == -1)
        {
            m_err.errno_code = errno;
            m_err.gp_code |= gpio_edge_error;
            return -1;
        }
    }

    // start thread if isr is not null and set the edge mode to create interrupt
    if (func != nullptr)
    {
        m_thread_running.test_and_set();
        m_err.errno_code =
            pthread_create(&m_isr_thread, nullptr, edgpio::_thread_exec, (void *)this);
        if (m_err.errno_code)
        {
            // error occured
            m_err.gp_code |= gpio_thread_start_error;
            m_thread_running.clear();
            return -1;
        }
    }
    return 0;
}

void edgpio::update()
{
    if (m_isr && m_isr_func != nullptr)
    {
        if (m_isr_func != nullptr)
        {
            m_isr_func();
        }
        else
        {
            wlog("No isr found - make sure one has been assigned");
        }
        m_isr = false;
    }
}

int edgpio::pin_num()
{
    return m_pin;
}

void * edgpio::_thread_exec(void * param)
{
    edgpio * _this = static_cast<edgpio *>(param);
    int pin = _this->m_pin;
    ilog("Starting gpio thread on pin {}", pin);
    _this->_exec();
    return nullptr;
}

int edgpio::read_pin()
{
    char buf[FILE_BUF_SZ];
    char r_val;
    int pin = m_pin;
    sprintf(buf, "/sys/class/gpio/gpio%d/value", pin);
    int fd = open(buf, O_RDONLY);
    if (fd == -1)
    {
        m_err.errno_code = errno;
        m_err.gp_code |= gpio_file_open_error;
        return -1;
    }
    else
    {
        int bytes = read(fd, &r_val, 1);
        close(fd);
        if (bytes == -1)
        {
            m_err.errno_code = errno;
            m_err.gp_code |= gpio_pin_error;
            return -1;
        }
        return int(r_val - '0');
    }
}

int edgpio::write_pin(int val)
{
    if (direction() == gpio_dir_in)
        return 0;

    char buf[FILE_BUF_SZ];
    char w_val = char('0' + val);
    int pin = m_pin;
    sprintf(buf, "/sys/class/gpio/gpio%d/value", pin);
    int fd = open(buf, O_WRONLY | O_NONBLOCK);
    if (fd == -1)
    {
        m_err.errno_code = errno;
        m_err.gp_code |= gpio_file_open_error;
        return -1;
    }
    else
    {
        int bytes = write(fd, &w_val, 1);
        close(fd);
        if (bytes == -1)
        {
            m_err.errno_code = errno;
            m_err.gp_code |= gpio_pin_error;
            return -1;
        }
    }
    return 0;
}

void edgpio::set_input_mode(gpio_input_mode md)
{
    char buf[FILE_BUF_SZ];
    int pin = m_pin;
    sprintf(buf, "/sys/class/gpio/gpio%d/active_low", pin);
    int fd = open(buf, O_WRONLY);
    if (fd == -1)
    {
        m_err.errno_code = errno;
        m_err.gp_code |= gpio_file_open_error;
        return;
    }
    else
    {
        int tmp = 0;
        switch (md)
        {
        case (active_high): // basically disable isr and don't start the thread!
            tmp = write(fd, "0", 1);
            close(fd);
        case (active_low): // trigger isr for both edges
            tmp = write(fd, "1", 1);
            break;
        }
        close(fd);
        if (tmp == -1)
        {
            m_err.errno_code = errno;
            m_err.gp_code |= gpio_file_input_mode_error;
        }
    }
}

void edgpio::set_output_mode(gpio_output_mode md)
{
    char buf[FILE_BUF_SZ];
    int pin = m_pin;
    sprintf(buf, "/sys/class/gpio/gpio%d/drive", pin);
    int fd = open(buf, O_WRONLY);
    if (fd == -1)
    {
        m_err.errno_code = errno;
        m_err.gp_code |= gpio_file_open_error;
        return;
    }
    else
    {
        int tmp = 0;
        switch (md)
        {
        case (gpio_strong): // basically disable isr and don't start the thread!
            tmp = write(fd, "strong", 6);
            close(fd);
        case (gpio_pullup): // trigger isr for both edges
            tmp = write(fd, "pullup", 6);
            break;
        case (gpio_pulldown): // trigger isr for rising edges only
            tmp = write(fd, "pulldown", 8);
            break;
        case (gpio_hiz):
            tmp = write(fd, "hiz", 3);
            break;
        }
        close(fd);
        if (tmp == -1)
        {
            m_err.errno_code = errno;
            m_err.gp_code |= gpio_output_mode_error;
        }
    }
}

void edgpio::_exec()
{
    char buf[FILE_BUF_SZ];
    char r_val;
    int pin = m_pin;
    sprintf(buf, "/sys/class/gpio/gpio%d/value", pin);
    pollfd polldes;
    polldes.fd = open(buf, O_RDONLY);
    polldes.events = POLLPRI | POLLERR;

    if (polldes.fd == -1)
    {
        int err = errno;
        elog("File descriptor is -1 : errno is {}", err);
        m_thread_running.clear();
    }

    while (m_thread_running.test_and_set())
    {
        if (poll(&polldes, 1, -1) == 1 && polldes.revents & POLLPRI)
        {
            lseek(polldes.fd, 0, SEEK_SET);
            read(polldes.fd, &r_val, 1);
            lseek(polldes.fd, 0, SEEK_SET);
            m_isr = true;
        }
    }
    close(polldes.fd);
    ilog("Ending gpio thread on pin {}", pin);
}

std::string edgpio::error_string(gpio_error_state gp_err)
{
    if (gp_err.gp_code == 0)
        return std::string("No error");
    std::string ret;
    if ((gp_err.gp_code & gpio_thread_start_error) == gpio_thread_start_error)
        ret += "GPIO Thread Start Error";
    else if ((gp_err.gp_code & gpio_thread_join_error) == gpio_thread_join_error)
        ret += " | GPIO Thread Join Error";
    else if ((gp_err.gp_code & gpio_unexport_error) == gpio_unexport_error)
        ret += " | GPIO Unexport Error";
    else if ((gp_err.gp_code & gpio_export_error) == gpio_export_error)
        ret += " | GPIO Export Error";
    else if ((gp_err.gp_code & gpio_direction_error) == gpio_direction_error)
        ret += " | GPIO Direction Error";
    else if ((gp_err.gp_code & gpio_edge_error) == gpio_edge_error)
        ret += " | GPIO Edge Error";
    else if ((gp_err.gp_code & gpio_pin_error) == gpio_pin_error)
        ret += " | GPIO Pin Error";
    else if ((gp_err.gp_code & gpio_output_mode_error) == gpio_output_mode_error)
        ret += " | GPIO Output Mode Error";
    else if ((gp_err.gp_code & gpio_file_open_error) == gpio_file_open_error)
        ret += " | GPIO File Open Error";
    else if ((gp_err.gp_code & gpio_file_input_mode_error) == gpio_file_input_mode_error)
        ret += " | GPIO Input Mode Error";
    else
        ret = "Unexpected error - gp_err.gp_code not zero yet value not recognized";
    if (ret[0] == ' ')
        ret.erase(ret.begin(), ret.begin() + 3); // get rid of " | " if its at the start of string
    ret += " - Errno message: " + std::string(strerror(gp_err.errno_code));
    return ret;
}
