#include "network.h"
#include "clientthread.h"
#include "user.h"
#include "util.h"

#include <sys/socket.h> 
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024

 void broadcast(User *user, void *arg)
{
	Message *msg = (Message *)arg;
	networkSend(user->sock, msg);
} 

void *clientthread(void *arg)
{
	User *self = (User *)arg;

	Message msg;

	debugEnable();
	debugPrint("Client thread started.");

	//TODO: Receive messages and send them to all users, skip self

	char buffer[BUFFER_SIZE];
	
	for(;;)
	{

		if(networkReceive(self->sock, &msg) < 0)
		{
			errnoPrint("konnte Nachricht nicht empfangen");
			break;
		}

		iterate_users(broadcast, &msg);
		
	}

	debugPrint("Client thread stopping.");
	debugDisable();

	remove_user(self);
	// free(arg); // speicher wird in user bereits freigegeben
	return NULL;
}
