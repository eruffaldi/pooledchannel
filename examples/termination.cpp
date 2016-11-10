#include "pooledchannel.hpp"
#include <array>
#include <signal.h>

bool done = false;

void
termination_handler (int signum)
{
	std::cout << "termination handler" << std::endl;
	done = true;
}

int main(int argc , char* argv[])
{
	struct sigaction new_action, old_action;

	/* Set up the structure to specify the new action. */
	new_action.sa_handler = termination_handler;
	sigemptyset (&new_action.sa_mask);
	new_action.sa_flags = 0;       
	sigaction (SIGUSR1, &new_action, NULL);	

	PooledChannel<int> pc(1,false,true);
	pc.set_termination(&done);
	
	int q = 0;
	std::cout << "starting read with wait\n" << std::endl;
	std::cout << "kill me sending -SIGUSR1\n" << std::endl;
	pc.read(q);
	std::cout << "exited and terminated? " << pc.is_terminated() << std::endl;
	return 0;
}