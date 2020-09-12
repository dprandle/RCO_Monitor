#pragma once

#include <time.h>
#include <inttypes.h>
#include <functional>

//! class edtimer 
/*! 
  This class keeps track of time allowing you to start, stop, and continue the timer.
  It also has a "dt" functionality to allow you to see how much time has elapsed since
  the last update call. This could be useful for various things.

  You can also set up the timer to execute a callback every so often (every so many milliseconds) or you can
  set it to execute once after some delay.
 */
class edtimer
{
  public:
	
	enum cb_mode {
		no_shot,
		single_shot,
		continous_shot
	};
	
	edtimer();
	~edtimer();
	
	void start();

	void update();

	std::function<void(edtimer*)> callback();

	cb_mode callback_mode();

	double callback_delay();

	void stop();
	
	void pause();

	void set_callback(std::function<void(edtimer*)> cb_func);

	void set_callback_mode(cb_mode mode);

	void set_callback_delay(double ms);
	
	double dt();

	bool running();

	double elapsed();

	bool paused();

	double elapsed_since_callback();

  private:
	double to_ms_(const timespec & tspec);
	timespec from_ms_(double ms);
	
	bool m_running;

	timespec m_start;
	timespec m_prev;
	timespec m_cur;
	timespec m_last_cb;
	timespec m_cb_delay;
	timespec m_pause;
	
	std::function<void(edtimer*)> m_cb;
	cb_mode m_cmode;
};

bool operator<(const timespec& lhs, const timespec& rhs);
bool operator<=(const timespec& lhs, const timespec& rhs);
bool operator==(const timespec& lhs, const timespec& rhs);
bool operator>=(const timespec& lhs, const timespec& rhs);
bool operator>(const timespec& lhs, const timespec& rhs);
timespec operator-(const timespec& lhs, const timespec& rhs);
timespec operator+(const timespec& lhs, const timespec& rhs);
