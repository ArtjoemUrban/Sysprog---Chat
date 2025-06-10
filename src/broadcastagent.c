#include <pthread.h>
#include <mqueue.h>
#include <semaphore.h>
#include "broadcastagent.h"
#include "util.h"
#include "network.h"
#include "user.h"


static mqd_t messageQueue;
static pthread_t threadId;
static sem_t pauseResumeSemaphore; // modul global

//broadcast thread
static void *broadcastAgent(void *arg)
{
	Server2Client msg;
	//TODO: Implement thread function for the broadcast agent here!
	for(;;)
	{
		sem_wait(&pauseResumeSemaphore);

		ssize_t recv = mq_receive(messageQueue, (char*)&msg, sizeof(S2C), NULL);
		if(recv >= 0)
		{
			sem_post(&pauseResumeSemaphore);
			infoPrint("MessageQueue hat nachricht erhalten");

			iterate_users(sendS2C, &msg);
			infoPrint("Nachricht wurde an alle verschickt");

		}else{
			errnoPrint("MessageQueue konnte nachricht nicht empfangen");
		}
	}
	return arg;
}

int broadcastAgentInit(void)
{
	if(sem_init(&pauseResumeSemaphore,0,1) != 0) // 0: wird nicht von Prozessen geteilt,  1: ist start wert
	{
		errnoPrint("konnte keinen semaphore erzeugen");
		returm -1;
	}
	//TODO: create message queue

	//TODO: start thread
	return -1;
}

void broadcastAgentCleanup(void)
{
	//TODO: stop thread
	pthread_cancel(threadId);
	pthread_join(threadId,NULL);
	//TODO: destroy message queue
	mq_close(messageQueue);
	mq_unlink("/broadcast_queue"); // von oben

	sem_destroy(&pauseResumeSemaphore); // semaphore zersören
}

void broadcastAgentPause(void)
{
	infoPrint("Chat wird Pausiert");
	sem_wait(&pauseResumeSemaphore);
	
}
void broadcastAgentResume(void)
{
	infoPrint("Chat wird fortgeführt");
	sem_post(&pauseResumeSemaphore);
}