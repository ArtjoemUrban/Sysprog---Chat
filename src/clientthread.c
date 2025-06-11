#include "network.h"
#include "clientthread.h"
#include "user.h"
#include "util.h"
#include "broadcastagent.h"

#include <arpa/inet.h>
#include <endian.h>
#include <sys/socket.h> 
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>

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
		errorPrint("LoginRequst konnte nicht Empfangen werden-> User wird entfernt");
		remove_user(self);
		return NULL;
	}
	// Prüfen ob Username nicht zu lange ist
	uint8_t name_len = loginRequest.header.len - sizeof(uint32_t) - sizeof(uint8_t); // 4 byte magic und 1 byte version
	
	// Prüfen ob die Version stimmt
	if(loginRequest.version != 0)
	{
		debugPrint("Ungültige Version des Protokolls: %i", loginRequest.version);
		sendLoginResponse(self->sock, PROTOCOL_VERSION_MISMATCH);
		remove_user(self);
		return NULL;
	}

	// Prüfen ob Username valide ist (in util.c)
	if(!isValidUsername(loginRequest.name, name_len))
	{
		debugPrint("Username nicht zugelassen");
		sendLoginResponse(self->sock, NAME_INVALID);
		remove_user(self);
		return NULL;
	}
	
	char name_cpy[USERNAME_MAX] ={0}; // setzt zu beginn alle bytes auf null
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
	self->name[USERNAME_MAX] = '\0'; // Um die Null-Terminierung noch mal zu garantieren 


	iterate_users(sendUserAddedtoALL,name_cpy); // sendet UserAdded an alle User
	iterate_users(sendUserListToNewUser, &(self->sock)); // sendet jeden Namen der aktiven user an den neuen

	infoPrint("User: %s wurde erfolgreich hinzugefügt", self->name);
	
 // ---------------------------------------------------------- Prüfung LRQ Ende -----------------------------------------------
	
	//TODO: Receive messages and send them to all users, skip self
	// Main Loop
	for(;;) 
	{
		Client2Server msg_c2s;
		int feedback =reciveC2S(self->sock, &msg_c2s);

		if(feedback ==1)
		{
			if(msg_c2s.text[0] == '/')
			{
				infoPrint("Command erhalten");
				//handle Command
			}else{
				Server2Client msg_s2c;
				createS2CMessage(&msg_s2c, self->name, msg_c2s.text, msg_c2s.header.len);
				// handleS2C(self->name, msg_c2s.text, msg_c2s.header.len);

				// TODO: an Broadcastagent queue senden
				mqd_t messageQueue = mq_open("/broadcast_queue", O_WRONLY);
				if (messageQueue == (mqd_t)-1) {
					errnoPrint("Fehler beim Öffnen der Message Queue");
					continue;	
				}
				// Sende die Nachricht an die Message Queue
				if(mq_send(messageQueue, (const char*)&msg_s2c, sizeof(Server2Client), 0) == -1)
				{
					errnoPrint("Fehler beim Senden der Nachricht an die Message Queue");
				}
				mq_close(messageQueue); // schließt die Message Queue
			}

		}
		else if( feedback == 0)
		{
			// client hat Verbindung beendet
			UserRemoved msg = createUserRemovedMessage(LEFT, self->name);
			iterate_users(sendUserRemoved,&msg);
			
			break;
		}
		else if(feedback == -1)
		{
			// Verbindung Abgebrochen
			UserRemoved msg = createUserRemovedMessage(ERROR, self->name);
			iterate_users(sendUserRemoved,&msg);
			break;
		}else
		{
			debugPrint("fehler beim empfangen der C2S versuch noch mal");
		}
	}
	remove_user(self);
	debugPrint("Client thread stopping it self.");
	debugDisable();
	pthread_exit(NULL);
	//free(arg); // speicher wird in user bereits freigegeben
	return NULL;
}
