#include "clientthread.h"
#include "user.h"
#include "util.h"

void *clientthread(void *arg)
{
	User *self = (User *)arg;

	debugPrint("Client thread started.");

	//TODO: Receive messages and send them to all users, skip self

	debugPrint("Client thread stopping.");
	free(arg);
	return NULL;
}
