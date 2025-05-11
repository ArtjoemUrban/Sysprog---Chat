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
	char *message = (char *)arg;
	send(user->sock, message, strlen(message),0);
}

void *clientthread(void *arg)
{
	User *self = (User *)arg;

	debugEnable();
	debugPrint("Client thread started.");

	//TODO: Receive messages and send them to all users, skip self

	char buffer[BUFFER_SIZE];
	
	for(;;)
	{
		ssize_t received = recv(self->sock, buffer, sizeof(buffer) -1, 0);
		if(received <= 0)
		{
			if(received == 0)
			{
				debugPrint("Client hat die Verbindung beendet");
			}else
			{
				errnoPrint("recv hat nicht funktioniert");
			}
			break;
		}
		buffer[received] = '\0'; // Null-Terminierung der Nachricht

		iterate_users(broadcast, buffer);
		
		memset(buffer, 0, sizeof(buffer)); // setzte alles im buffer auf '0'
	}

	debugPrint("Client thread stopping.");
	debugDisable();

	remove_user(self);
	// free(arg); // speicher wird in user bereits freigegeben
	return NULL;
}
