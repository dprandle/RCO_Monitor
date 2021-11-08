#include <signal.h>

#include "main_control.h"
#include "timer.h"
#include "callback.h"
#include "rce_serial_comm.h"
#include "radio_telnet.h"
#include "logger.h"

void sig_exit_handler(int signum)
{
    edm.stop();
}

int32_t main(int32_t argc, char * argv[])
{
    signal(SIGTERM, sig_exit_handler);
    signal(SIGINT, sig_exit_handler);
    signal(SIGKILL, sig_exit_handler);
    
    edm.add_subsystem<RCE_Serial_Comm>();
    edm.add_subsystem<Radio_Telnet>();
    edm.start();
    return 0;
}
