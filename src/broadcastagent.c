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

		ssize_t recv = mq_receive(messageQueue, (char*)&msg, sizeof(Server2Client), NULL);
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
		return -1;
	}
	//TODO: create message queue
	// Struct für Kapazität der Queue
	struct mq_attr attr = 
	{
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = sizeof(Server2Client), // Größe der Nachricht
        .mq_curmsgs = 0
    };

	messageQueue = mq_open("/broadcast_queue", O_CREAT | O_RDWR, 0644, &attr); // Message Queue wird erzeugt 
    if (messageQueue == (mqd_t)-1) {
        errnoPrint("mq_open");
        return -1;
    }

	//TODO: start thread
	if (pthread_create(&threadId, NULL, broadcastAgent, NULL) != 0) {
        errnoPrint("pthread_create");
        mq_close(messageQueue);
        mq_unlink("/broadcast_queue");
        return -1;
    }

	infoPrint("Broadcast agent initialized successfully");
	return 0;
}

void broadcastAgentCleanup(void)
{
	pthread_cancel(threadId);
	pthread_join(threadId,NULL);
	
	mq_close(messageQueue);
	mq_unlink("/broadcast_queue"); // von oben



	sem_destroy(&pauseResumeSemaphore);
	infoPrint("Broadcast agent cleanup completed");
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