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

// Funktion für Client
void *clientthread(void *arg)
{
	User *self = (User *)arg;

	debugEnable();
	debugPrint("Client thread started.");
 /// --------------------------------------------------------- Prüfung des Login Requests ------------------------------------
	LoginRequest loginRequest; // eventuell speicher mit malloc() verwalten -> kein Stack over flow

	if(!reciveLoginRequest(self->sock, &loginRequest))
	{
		debugPrint("Fehler beim Empfangen des LRQ");
		errorPrint("LoginRequst konnte nicht Empfangen werden-> User wird entfernt");
		remove_user(self);
		return NULL;
	}
	// Prüfen ob Username nicht zu lange ist
	uint8_t name_len = loginRequest.header.len - sizeof(uint32_t) - sizeof(uint8_t); // 4 byte magic und 1 byte version
	
	// Prüfen ob die Version stimmt
	if(loginRequest.version != 0)
	{
		debugPrint("Ungültige Version des Protokolls");
		sendLoginResponse(self->sock, PROTOCOL_VERSION_MISMATCH);
		remove_user(self);
		return NULL;
	}

	// Prüfen ob Username valide ist
	if(!isValidUsername(loginRequest.name, name_len))
	{
		debugPrint("Username nicht gueltig");
		sendLoginResponse(self->sock, NAME_INVALID);
		remove_user(self);
		return NULL;
	};
	
	char name_cpy[NAME_FINAL] ={0}; // setzt zu beginn alle bytes auf null
	memcpy(name_cpy, loginRequest.name, name_len);
	name_cpy[name_len] = '\0'; // Null termenierung anhaengen

	if(isNameTaken(name_cpy))
	{
		debugPrint("Name %s ist bereits vergeben", name_cpy);
		sendLoginResponse(self->sock, NAME_TAKEN);
		remove_user(self);
		return NULL;
	}

	sendLoginResponse(self->sock, SUCCESS);
	memcpy(self->name, name_cpy, name_len+1); // ??? evtl mutex 
	self->name[NAME_FINAL] = '\0'; // Um die Null-Terminierung noch mal zu garantieren 

	debugPrint("LoginResponse verschickt");

	iterate_users(sendUserAddedtoALL,name_cpy); // sendet UserAdded an alle User
	iterate_users(sendUserListToNewUser, &(self->sock)); // sendet jeden Namen der aktiven user an den neuen

	infoPrint("User: %s wurde erfolgreich regestriert", self->name);
	
 // ---------------------------------------------------------- Prüfung LRQ Ende -----------------------------------------------
	
	//TODO: Receive messages and send them to all users, skip self
	
	for(;;) // Main Loop
	{
		int buffer;
		ssize_t feedback = recv(self->sock, buffer, sizeof(buffer), 0); // später recive C2S

		if(feedback > 0)
		{
			// alles okay
			// handle S2C
		}
		else if( feedback == 0)
		{
			// client hat Verbindung beendet
			UserRemoved msg = createUserRemovedMessage(LEFT, self->name);
			iterate_users(sendUserRemoved,&msg);
			
			break;
		}
		else
		{
			// Verbindung Abgebrochen
			UserRemoved msg = createUserRemovedMessage(ERROR, self->name);
			iterate_users(sendUserRemoved,&msg);
			break;
		}	
	}
	remove_user(self);
	debugPrint("Client thread stopping.");
	debugDisable();
	pthread_exit(NULL);
	//free(arg); // speicher wird in user bereits freigegeben
	return NULL;
}
