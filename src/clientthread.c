#include "network.h"
#include "clientthread.h"
#include "user.h"
#include "util.h"

#include <arpa/inet.h>
#include <endian.h>
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

// Funktion für Client
void *clientthread(void *arg)
{
	User *self = (User *)arg;

	Message msg;
	debugEnable();

	LoginRequest loginRequest;

	if(!reciveLoginRequest(self->sock,  &loginRequest))
	{
		remove_user(self);
		return NULL;
	}


	// Prüfen ob Username nicht zu lange ist
	uint16_t lrq_len = ntohs(loginRequest.header.len); // länge des LRQ ohne Header
	if( lrq_len < 5 || lrq_len > (5 + USER_NAME_MAX)) // 5 bytes sind magic und version
	{
		debugPrint("Username zu Lang oder Kurz");
		remove_user(self);
		return NULL;
	}
	// Prüfen ob Username valide ist
	uint16_t name_len = lrq_len - 5;
	if(!isValidUsername(loginRequest.name, name_len))
	{
		debugPrint("Username enthält ungültige zeichen");
		sendLoginResponse(self->sock, NAME_INVALID);
		remove_user(self);
	};

	
	debugPrint("Client thread started.");

	//TODO: Receive messages and send them to all users, skip self

	char buffer[BUFFER_SIZE];
	
	for(;;)
	{

		if(networkReceive(self->sock, &msg) < 0)
		{
			debugPrint("konnte Nachricht nicht empfangen");
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
