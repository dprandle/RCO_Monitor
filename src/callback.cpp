#include "callback.h"
#include "timer.h"

void Wait_Ready_Callback::exec()
{
	timer->stop();
}
