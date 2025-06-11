#include <stdlib.h>
#include <getopt.h>
#include <signal.h>

#include "connectionhandler.h"
#include "util.h"
#include "broadcastagent.h"
#include "user.h"

#define DEFAULT_PORT 8111
/*
// Signal handler für SIGINT und SIGTERM -> um den Server sauber zu beenden
void signalHandler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		infoPrint("Received termination signal, cleaning up and exiting");
		broadcastAgentCleanup();
		destroyUserList();
		exit(EXIT_SUCCESS);
	}
}*/

int main(int argc, char **argv)
{
	int port = DEFAULT_PORT;
	utilInit(argv[0]);
	infoPrint("Chat server, group 12");	//TODO: Add your group number!

	//TODO: evaluate command line arguments
	if (argc > 1) {
		port = atoi(argv[1]); // atoi converts string to integer
		if (port <= 1023 || port > 65535) // Check if port is in valid range
		{
			infoPrint("Invalid port number: %s. Using default port %d.", argv[1], DEFAULT_PORT);
			port = DEFAULT_PORT;
		} else {
			infoPrint("Using port: %d", port);
		}
	}
	else
	{
		infoPrint("Starting Server with default port: %d.", DEFAULT_PORT);
	}
	
	//TODO: perform initialization
	
	

	//TODO: use port specified via command line
	const int result = connectionHandler((in_port_t)port);
	const int broadcast = broadcastAgentInit();
	if( broadcast != 0)
	{
		errorPrint("Broadcast agent initialization failed");
		return EXIT_FAILURE; 
	}

	//TODO: perform cleanup, if required by your implementation
	return result != -1 ? EXIT_SUCCESS : EXIT_FAILURE; // Return success if connectionHandler was successful, otherwise failure
}
