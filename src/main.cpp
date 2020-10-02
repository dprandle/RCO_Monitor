#include <signal.h>

#include "main_control.h"
#include "timer.h"
#include "callback.h"
#include "rce_serial_comm.h"
#include "logger.h"

void sig_exit_handler(int signum)
{
    edm.stop();
    exit(0);
}

int32_t main(int32_t argc, char * argv[])
{
    signal(SIGTERM, sig_exit_handler);
    signal(SIGINT, sig_exit_handler);
    signal(SIGKILL, sig_exit_handler);

    int32_t port = 0;
	for (int32_t i = 0; i < argc; ++i)
	{
		std::string curarg(argv[i]);
		if (curarg.find("-port:") == 0)
			port = std::stoi(curarg.substr(6));
	}
    
    edm.add_subsystem<RCE_Serial_Comm>();
	edm.start();
	
    return 0;
}
