#include <pthread.h>
#include <mqueue.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>

#include "broadcastagent.h"
#include "util.h"
#include "network.h"
#include "user.h"

#define BROADCAST_QUEUE_NAME "/broadcast_queue"

static mqd_t messageQueue;
static pthread_t threadId;
static sem_t pauseResumeSemaphore; // modul global
 

static volatile bool isRunning = true;
static bool isInitialized = false;

//broadcast thread
static void *broadcastAgent(void *arg)
{
	Server2Client msg;
	while(isRunning) // solange der Thread läuft
	{
		infoPrint("Warte auf Nachricht in der MessageQueue");
		ssize_t recv = mq_receive(messageQueue, (char*)&msg, sizeof(Server2Client), NULL);
		if(recv >= 0)
		{
			 
			infoPrint("MessageQueue hat nachricht erhalten.");
			sem_wait(&pauseResumeSemaphore); // warten bis der Thread fortgesetzt wird

			pthread_mutex_lock(&userLock); // Sperre für die User-Liste
			iterate_users(sendS2C, &msg);
			pthread_mutex_unlock(&userLock); // Sperre wieder freigeben
			
			sem_post(&pauseResumeSemaphore); // fortsetzen
		}else{
			errnoPrint("MessageQueue konnte nachricht nicht empfangen: ");
		}
	}
	return arg;
}

int broadcastAgentInit(void)
{
	if(isInitialized) // falls der Agent bereits initialisiert wurde
	{
		infoPrint("Broadcast agent is already initialized.");
		return 0;
	}
	if(sem_init(&pauseResumeSemaphore,0,1) != 0) // 0: wird nicht von Prozessen geteilt,  1: ist start wert
	{
		errnoPrint("Broadcast agent: konnte keinen semaphore erzeugen!!!");
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

	messageQueue = mq_open(BROADCAST_QUEUE_NAME, O_CREAT | O_RDWR, 0644, &attr); // Message Queue wird erzeugt 
    if (messageQueue == (mqd_t)-1) {
        errnoPrint("mq_open");
        return -1;
    }

	//TODO: start thread
	if (pthread_create(&threadId, NULL, broadcastAgent, NULL) != 0) {
        errnoPrint("pthread_create");
        mq_close(messageQueue);
        mq_unlink(BROADCAST_QUEUE_NAME); // Message Queue wird gelöscht
        return -1;
    }

	infoPrint("Broadcast agent initialized successfully.");
	isInitialized = true; // Initialisierung erfolgreich
	return 0;
}

void broadcastAgentCleanup(void)
{
	isRunning = false; // Thread beenden
	sem_post(&pauseResumeSemaphore); // falls der Thread auf eine Nachricht wartet
	pthread_cancel(threadId); // Thread beenden
	pthread_join(threadId,NULL);
	
	mq_close(messageQueue);
	mq_unlink(BROADCAST_QUEUE_NAME); // von oben



	sem_destroy(&pauseResumeSemaphore);
	infoPrint("Broadcast agent cleanup completed.");
}

void broadcastAgentPause(void)
{
	infoPrint("Chat wird Pausiert.");
	sem_wait(&pauseResumeSemaphore); // versuche Semaphore zu nehmen, falls nicht möglich, ist der Agent bereits pausiert
	
	
}
void broadcastAgentResume(void)
{
	infoPrint("Chat wird fortgeführt.");
	sem_post(&pauseResumeSemaphore);
}