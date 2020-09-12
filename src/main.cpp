#include <edmctrl.h>
#include <stdlib.h>
#include <signal.h>

#include <edtimer.h>
#include <edcallback.h>
#include <edcomm_system.h>
#include <edlight_system.h>

#include <sys/types.h>
#include <unistd.h>

void sig_exit_handler(int signum)
{
    std::string msg("Cought signal ");
    switch (signum)
    {
    case(SIGTERM):
        msg += "sigterm";
        break;
    case(SIGINT):
        msg += "sigint";
        break;
    case(SIGKILL):
        msg += "sigkill";
        break;
    default:
        return;
    }
    edm.quit();
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
	
    edm.add_sys<edcomm_system>()->set_port(port);
    edm.add_sys<edlight_system>();
	
	edm.start();
    edm.init();

    while (edm.running())
		edm.update();
	
    return 0;
}
