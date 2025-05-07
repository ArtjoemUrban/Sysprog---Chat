#ifndef USER_H
#define USER_H

#include <pthread.h>

typedef struct User
{
	
	struct User *prev;
	struct User *next;
	pthread_t thread;	//thread ID of the client thread
	int sock;		//socket for client
} User;

//TODO: Add prototypes for functions that fulfill the following tasks:
// * Add a new user to the list and start client thread
// * Iterate over the complete list (to send messages to all users)
// * Remove a user from the list
//CAUTION: You will need proper locking!


void broadcast_message(const char *message);
void cleanup_user_list(void);


User *listADD(pthread_t *thread, int client_sock);
void listForEach(void (*func)(User *));
int listRemoveByName(const char *name);
void listRemoveAll(void);

#endif
