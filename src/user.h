#ifndef USER_H
#define USER_H

#include <stdbool.h>
#include <pthread.h>

extern pthread_mutex_t userLock; // Mutex für die User-Liste
//extern bedutet, dass die Variable in einer anderen Datei definiert ist


typedef struct User
{
	
	struct User *prev;
	struct User *next;
	pthread_t thread;	//thread ID of the client thread
	int sock;		//socket for client
	char name [32]; // Null terminiert
	bool isAdmin; // true, wenn der User Admin ist
	
} User;


// Fügt einen neuen Benutzer zur Liste hinzu und startet dessen Client-Thread
User *add_user(int sock);

// Entfernt einen Benutzer aus der Liste
void remove_user(User *user);

// Iteriert über alle Benutzer in der Liste und ruft für jeden eine Callback-Funktion auf
void iterate_users(void (*callback)(User *user, void *arg), void *arg);

// Prüfe ob Username bereits vorhanden
User *isNameTaken(const char* newName);

int kickUser(const char *username);

void destroyUserList(void);

#endif
