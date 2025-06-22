#define _POSIX_C_SOURCE 199309L
#define _POSIX_C_SOURCE 200809L

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
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    
    ts.tv_sec += timeoutMillis / 1000;
    ts.tv_nsec += (timeoutMillis % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    int ret = mq_timedsend(mq, msg, len, prio, &ts);
    if (ret == -1) {
        if (errno == ETIMEDOUT) {
            errnoPrint("Message queue is full, and timeout expired.");
			return -1; // Fehler beim Senden
        } else {
            errnoPrint("mq_timedsend failed");
			return -2; // Fehler beim Senden
        }
		
    }

    return ret;
}

// Funktion für Client
void *clientthread(void *arg)
{
	User *self = (User *)arg;
 /// --------------------------------------------------------- Prüfung des Login Requests ------------------------------------
	LoginRequest loginRequest; // eventuell speicher mit malloc() verwalten -> kein Stack over flow

	if(!reciveLoginRequest(self->sock, &loginRequest))
	{
		errorPrint("LoginRequst konnte nicht Empfangen werden-> User wird entfernt");
		//pthread_mutex_unlock(&userLock); ist bereits
		remove_user(self);
		
		return NULL;
	}
	// Prüfen ob Username nicht zu lange ist
	uint8_t name_len = loginRequest.header.len - sizeof(uint32_t) - sizeof(uint8_t); // 4 byte magic und 1 byte version
	
	// Prüfen ob die Version stimmt
	if(loginRequest.version != 0)
	{
		errorPrint("Ungültige Version des Protokolls: %i", loginRequest.version);
		sendLoginResponse(self->sock, PROTOCOL_VERSION_MISMATCH);
		pthread_mutex_unlock(&userLock);
		pthread_mutex_lock(&userLock);
		remove_user(self);
		return NULL;
	}

	// Prüfen ob Username valide ist (in util.c)
	if(!isValidUsername(loginRequest.name, name_len))
	{
		debugPrint("Username nicht zugelassen");
		sendLoginResponse(self->sock, NAME_INVALID);
		pthread_mutex_unlock(&userLock);
		pthread_mutex_lock(&userLock);
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
		pthread_mutex_unlock(&userLock);
		pthread_mutex_lock(&userLock);
		remove_user(self);
		return NULL;
	}

	sendLoginResponse(self->sock, SUCCESS);
	memcpy(self->name, name_cpy, name_len+1); // ??? evtl mutex 
	self->name[USERNAME_MAX] = '\0'; // Um die Null-Terminierung noch mal zu garantieren 

	if( strcmp(self->name, "Admin") == 0)
	{
		self->isAdmin = true; // wenn der User admin ist
		infoPrint("Admin has joined!!!");
	}

	iterate_users(sendUserAddedtoALL,name_cpy); // sendet UserAdded an alle User
	iterate_users(sendUserListToNewUser, &(self->sock)); // sendet jeden Namen der aktiven user an den neuen

	infoPrint("User: %s wurde erfolgreich hinzugefügt", self->name);
	
	pthread_mutex_unlock(&userLock); // Gibt Mutex nach dem Hinzufügen des Users frei
	
 // ---------------------------------------------------------- Prüfung LRQ Ende -----------------------------------------------
	int isRunning = 1; //
	//TODO: Receive messages and send them to all users, skip self
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
						createS2CMessage(&msg_s2c, "", "Chat wurde pausiert.", 20);
						pthread_mutex_lock(&userLock); // Sperre für die User-Liste
						iterate_users(sendS2C, &msg_s2c);
						pthread_mutex_unlock(&userLock); // Sperre wieder freigeben
						isRunning = 0; 
						continue;
					}else{
						sendS2CError(self->sock, "Chat ist bereits pausiert.");
						errorPrint("chat ist bereits pausiert -> verwende zuerst /resume.");
						continue;
					}

				}else if (msg_c2s.header.len == 7  && memcmp(msg_c2s.text, "/resume", 7) == 0)
				{
					if (isRunning == 0)
					{
						broadcastAgentResume();
						Server2Client msg_s2c;
						createS2CMessage(&msg_s2c, "", "Chat  wird fortgeführt.", 20);
						pthread_mutex_lock(&userLock);
						iterate_users(sendS2C, &msg_s2c);
						pthread_mutex_unlock(&userLock); 
						isRunning = 1;
						continue;
					}
					else{
						sendS2CError(self->sock, "Chat ist nicht pausiert -> verwende zuerst /pause.");
						errorPrint("chat ist nicht pausiert");
						continue;
					}
				}else if(msg_c2s.header.len >= 5 && memcmp(msg_c2s.text, "/kick",5)==0)
				{
					infoPrint("kick Befehl erhalten");
					char username[USERNAME_MAX] = {0};
					memcpy(username, msg_c2s.text + 6, msg_c2s.header.len - 6); // kopiert den Namen nach dem /kick
					if( kickUser(username))
					{
						UserRemoved msg = createUserRemovedMessage(KICKED, username);
						pthread_mutex_lock(&userLock); 
						iterate_users(sendUserRemoved,&msg);
						pthread_mutex_unlock(&userLock); 
						continue;

					}else
					{
						sendS2CError(self->sock, "User existiert nicht.");
					}
					continue;


				}	
			}else{
				//infoPrint("Text erhalten: %s", msg_c2s.text);
				Server2Client msg_s2c;
				createS2CMessage(&msg_s2c, self->name, msg_c2s.text, msg_c2s.header.len);
				//sendS2CError(self->sock, "Text wurde empfangen und wird an alle User gesendet.");

				mqd_t messageQueue = mq_open("/broadcast_queue", O_WRONLY);
				if (messageQueue == (mqd_t)-1) {
					errnoPrint("Fehler beim Öffnen der Message Queue");
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
			// client hat Verbindung beendet
			//self->sock = -1; // setze sock auf -1 damit der User nicht mehr in der Liste ist

			UserRemoved msg = createUserRemovedMessage(LEFT, self->name);
			pthread_mutex_lock(&userLock);

			char user[4] = "user";
			
			if (memcmp(self->name, user, 4) != 0) // 
			{
				iterate_users(sendUserRemoved,&msg); // -> hier ist ein problem
			}
			

			
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
	//free(arg); // speicher wird in user bereits freigegeben
	return NULL;
}
