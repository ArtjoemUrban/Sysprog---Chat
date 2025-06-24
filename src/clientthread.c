#define _POSIX_C_SOURCE 200809L // für mq_timedsend

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
#include <time.h>
#include <errno.h>

// Funktion zum Senden einer Nachricht an die Message Queue mit Timeout
int send2MessageQueueWithTimeout(mqd_t mq, const void* msg, size_t len, unsigned int prio, int timeoutMillis)
{
    struct timespec ts; // Struktur für die Zeitangabe
    clock_gettime(CLOCK_REALTIME, &ts); // Aktuelle Zeit abrufen
    
    ts.tv_sec += timeoutMillis / 1000; // Sekunden hinzufügen
    ts.tv_nsec += (timeoutMillis % 1000) * 1000000; // Nanosekunden hinzufügen
	// Überprüfen, ob die Nanosekunden korrekt sind
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

	// Sende die Nachricht an die Message Queue mit Timeout
    int sent = mq_timedsend(mq, msg, len, prio, &ts); 
    if (sent == -1) {
        if (errno == ETIMEDOUT) {
            errnoPrint("Message queue is full, and timeout expired.");
			return -1; // Fehler beim Senden
        } else {
            errnoPrint("mq_timedsend failed");
			return -2; // Fehler beim Senden
        }	
    }
    return sent;
}

void *clientthread(void *arg)
{
	User *self = (User *)arg;

 /// --------------------------------------------------------- Prüfung des Login Requests ------------------------------------
	LoginRequest loginRequest;

	if(!reciveLoginRequest(self->sock, &loginRequest))
	{
		errorPrint("LoginRequst konnte nicht Empfangen werden-> User wird entfernt");
		//pthread_mutex_unlock(&userLock);
		remove_user(self);
		return NULL;
	}

	// Prüfen ob die Version stimmt
	if(loginRequest.version != 0)
	{
		errorPrint("Ungültige Version des Protokolls: %i", loginRequest.version);
		sendLoginResponse(self->sock, PROTOCOL_VERSION_MISMATCH);
		remove_user(self);
		return NULL;
	}

	uint8_t name_len = loginRequest.header.len - sizeof(uint32_t) - sizeof(uint8_t); // 4 byte magic und 1 byte version
	// Prüfen ob Username valide ist (in util.c)
	if(!isValidUsername(loginRequest.name, name_len))
	{
		errorPrint("Username nicht zugelassen");
		sendLoginResponse(self->sock, NAME_INVALID);
		remove_user(self);
		return NULL;
	}
	
	char name_cpy[USERNAME_MAX] ={0}; // setzt zu beginn alle bytes auf null
	memcpy(name_cpy, loginRequest.name, name_len);
	name_cpy[name_len] = '\0'; // Null termenierung anhaengen

	if(isNameTaken(name_cpy))
	{
		errorPrint("Name %s ist bereits vergeben", name_cpy);
		sendLoginResponse(self->sock, NAME_TAKEN);
		remove_user(self);
		return NULL;
	}
	// ---------------------------------------------------------- Prüfung LRQ Ende -----------------------------------------------
	
	sendLoginResponse(self->sock, SUCCESS);
	memcpy(self->name, name_cpy, name_len+1);
	self->name[USERNAME_MAX] = '\0'; // Um die Null-Terminierung noch mal zu garantieren 

	if( strcmp(self->name, "Admin") == 0)
	{
		self->isAdmin = true; // wenn der User admin ist
		debugPrint("Admin has joined!!!");
	}

	iterate_users(sendUserAddedtoALL,name_cpy); // sendet UserAdded an alle User
	iterate_users(sendUserListToNewUser, &(self->sock)); // sendet jeden Namen der aktiven user an den neuen

	debugPrint("User: %s wurde erfolgreich hinzugefügt", self->name);
	
	pthread_mutex_unlock(&userLock); // Gibt Mutex nach dem Hinzufügen des Users frei
	
 
	int isRunning = 1; // für pause und resume
	for(;;) 
	{
		Client2Server msg_c2s; 
		int recv =reciveC2S(self->sock, &msg_c2s);

		if(recv ==1)
		{
			if(msg_c2s.text[0] == '/')
			{
				if(self->isAdmin != true)
				{
					errorPrint("User %s tried to use a command but is not an admin", self->name);
					sendS2CError(self->sock, "Du bist kein Admin und kannst keine Befehle ausführen.");
					continue;
				}else if (msg_c2s.header.len == 6  && memcmp(msg_c2s.text, "/pause", 6) == 0)
				{
					if (  isRunning == 1)
					{
						broadcastAgentPause();
						infoPrint("Broadcast Agent is Paused");
						Server2Client msg_s2c;
						createS2CMessage(&msg_s2c, "", "Chat wurde pausiert.", 20); // Sender Leer da von Server
						pthread_mutex_lock(&userLock);
						iterate_users(sendS2C, &msg_s2c);
						pthread_mutex_unlock(&userLock);
						isRunning = 0;
						continue;
					}else{
						sendS2CError(self->sock, "Chat ist bereits pausiert.");
						continue;
					}

				}else if (msg_c2s.header.len == 7  && memcmp(msg_c2s.text, "/resume", 7) == 0)
				{
					if (isRunning == 0)
					{
						broadcastAgentResume();
						Server2Client msg_s2c;
						createS2CMessage(&msg_s2c, "", "Chat wird fortgeführt.", 22);
						pthread_mutex_lock(&userLock);
						iterate_users(sendS2C, &msg_s2c);
						pthread_mutex_unlock(&userLock); 
						isRunning = 1;
						continue;
					}
					else{
						sendS2CError(self->sock, "Chat ist nicht pausiert -> verwende zuerst /pause.");
						continue;
					}
				}else if(msg_c2s.header.len >= 5 && memcmp(msg_c2s.text, "/kick",5)==0)
				{
					debugPrint("kick Befehl erhalten");
					char username[USERNAME_MAX] = {0};
					memcpy(username, msg_c2s.text + 6, msg_c2s.header.len - 6); // kopiert den Namen nach dem /kick
					int response = kickUser(username); // versucht den User zu kicken
					if( response == 1)
					{
						UserRemoved msg = createUserRemovedMessage(KICKED, username);
						pthread_mutex_lock(&userLock); 
						iterate_users(sendUserRemoved,&msg);
						pthread_mutex_unlock(&userLock); 
						continue;

					}else if(response ==2)
					{
						sendS2CError(self->sock, "User exestiert nicht.");
					}else if(response == 3)
					{
						sendS2CError(self->sock, "Admin kann nicht gekickt werden.");
					}
					continue;


				}	
			}else{
				Server2Client msg_s2c;
				createS2CMessage(&msg_s2c, self->name, msg_c2s.text, msg_c2s.header.len);

				mqd_t messageQueue = mq_open("/broadcast_queue", O_WRONLY);
				if (messageQueue == (mqd_t)-1) {
					errnoPrint("Fehler beim Öffnen der Message Queue");
					sendS2CError(self->sock, "Message Queue konnte nicht geöffnet werden.");
					continue;	
				}
				// Sende die Nachricht an die Message Queue
				if(send2MessageQueueWithTimeout(messageQueue, (const char*)&msg_s2c, sizeof(Server2Client),0,100) == -1)
				{
					errnoPrint("Fehler beim Senden der Nachricht an die Message Queue");
					sendS2CError(self->sock, "Message Queue ist voll diese Nachricht wird nicht gesendet.");
				}
				mq_close(messageQueue); // schließt die Message Queue
			}

		}
		else if( recv == 0)
		{
			// Verbindung wurde geschlossen
			UserRemoved msg = createUserRemovedMessage(LEFT, self->name);
			pthread_mutex_lock(&userLock);

			iterate_users(sendUserRemoved,&msg);
			
			remove_user(self);
			pthread_exit(NULL);
			break;
		}
		else if(recv == -1)
		{
			// Verbindung Abgebrochen
			errorPrint("Verbindung zum Client %s wurde abgebrochen", self->name);
			UserRemoved msg = createUserRemovedMessage(ERROR, self->name);
			pthread_mutex_lock(&userLock);

			iterate_users(sendUserRemoved,&msg);
			remove_user(self);
			pthread_exit(NULL);
			break;
		}else
		{
			errorPrint("fehler beim empfangen der C2S versuch noch mal");
		}
	}	
	debugPrint("Client thread stopping it self.");
	pthread_exit(NULL);
	return NULL;
}
