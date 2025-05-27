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
/// --------------------------------------------------------- Prüfung des Login Requests ------------------------------------
	LoginRequest loginRequest;

	if(!reciveLoginRequest(self->sock, &loginRequest))
	{
		debugPrint("Kein validen LRQ erhalten");
		remove_user(self);
		return NULL;
	}
	// Prüfen ob Username nicht zu lange ist
	uint8_t name_len = loginRequest.header.len - sizeof(uint32_t) - sizeof(uint8_t); // 4 byte magic und 1 byte version
	
	// Prüfen ob Username valide ist
	if(!isValidUsername(loginRequest.name, name_len))
	{
		debugPrint("Username nicht gueltig");
		sendLoginResponse(self->sock, NAME_INVALID);
		remove_user(self);
		return NULL;
	};
	// Prüfen ob die Version stimmt
	if(loginRequest.version != 0)
	{
		debugPrint("Ungültige Version des Protokolls");
		sendLoginResponse(self->sock, PROTOCOL_VERSION_MISMATCH);
		remove_user(self);
		return NULL;
	}
	char name_cpy[USER_NAME_MAX +1] ={0}; // setzt zu beginn alle bytes auf null
	memcpy(name_cpy, loginRequest.name, name_len);
	name_cpy[name_len] = '\0'; // Null termenierung anhaengen

	if(isNameTaken(name_cpy))
	{
		debugPrint("Name ist bereits vergeben");
		sendLoginResponse(self->sock, NAME_TAKEN);
		remove_user(self);
		return NULL;
	}

	sendLoginResponse(self->sock, SUCCESS);
	memcpy(self->name, name_cpy, USER_NAME_MAX +1);

// ---------------------------------------------------------- Prüfung LRQ Ende -----------------------------------------------

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
