#pragma once

#include <pthread.h>
#include <atomic>
#include <string>
#include <functional>

#define FILE_BUF_SZ 40

const int GPIO_12 = 12;
const int GPIO_13 = 13;
const int GPIO_14 = 14;
const int GPIO_15 = 15;
const int GPIO_44 = 44;
const int GPIO_45 = 45;
const int GPIO_46 = 46;
const int GPIO_47 = 47;
const int GPIO_48 = 48;
const int GPIO_49 = 49;


enum gpio_dir {
	gpio_dir_out,
	gpio_dir_in,
	gpio_dir_out_high,
	gpio_dir_out_low
};

enum gpio_output_mode
{
	gpio_strong,
	gpio_pullup,
	gpio_pulldown,
	gpio_hiz
};

enum gpio_input_mode
{
	active_high,
	active_low
};

enum gpio_isr_edge
{
	gpio_edge_none,
	gpio_edge_both,
	gpio_edge_rising,
	gpio_edge_falling
};

enum gpio_error_code
{
	gpio_no_error,
	gpio_thread_start_error=1,
	gpio_thread_join_error=2,
	gpio_unexport_error=4,
	gpio_export_error=8,
	gpio_direction_error=16,
	gpio_edge_error=32,
	gpio_pin_error=64,
	gpio_output_mode_error=128,
	gpio_file_open_error=256,
	gpio_file_input_mode_error=512
};

struct gpio_error_state
{
	gpio_error_state():
		gp_code(0),
		errno_code(0)
	{}
	
	int gp_code;
	int errno_code;
};

struct pwm_measurement
{
    int8_t cur_edge; // This will be 0 for rising and 1 for falling - useful mainly for when the edge mode is both
    int8_t prev_edge; // What period are we measuring - falling to falling, rising to rising, or both
    double seconds; // the time from the last edge to this one
};

class edgpio
{
  public:

	edgpio(int pin);
	~edgpio();
	
	int set_direction(gpio_dir dir);
	
	int direction();

	void set_output_mode(gpio_output_mode md);

	void set_input_mode(gpio_input_mode md);

    int set_isr(gpio_isr_edge edge, std::function<void()> func);

	int read_pin();

	int write_pin(int val);
	
	void update();

	int pin_num();

	gpio_error_state get_and_clear_error();

	static std::string error_string(gpio_error_state gp_err);

  private:

	static void * _thread_exec(void * param);
	void _exec();

	std::function<void()> m_isr_func;
	gpio_error_state m_err;

    std::atomic_int m_pin;
    std::atomic_flag m_thread_running = ATOMIC_FLAG_INIT;
	pthread_t m_isr_thread;
    std::atomic_bool m_isr;
};
