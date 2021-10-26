#pragma once

struct Callback
{
	virtual ~Callback() {}
	virtual void exec()=0;
};

class Timer;

struct Timer_Callback : public Callback
{
	Timer * timer;
};

struct Wait_Ready_Callback : public Timer_Callback
{
	virtual void exec();
};
