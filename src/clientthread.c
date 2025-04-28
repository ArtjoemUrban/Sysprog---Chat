#include "clientthread.h"
#include "user.h"
#include "util.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024

void *clientthread(void *arg)
{
	User *self = (User *)arg;

	debugPrint("Client thread started.");

	//TODO: Receive messages and send them to all users, skip self

	char buffer[BUFFER_SIZE];
	for(;;)
	{
		memset(buffer, 0, sizeof(buffer));
	}

	debugPrint("Client thread stopping.");
	free(arg);
	return NULL;
}
