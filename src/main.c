#define _POSIX_C_SOURCE 199506L // Für POSIX sigwait
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#include "connectionhandler.h"
#include "util.h"
#include "broadcastagent.h"
#include "user.h"

#define DEFAULT_PORT 8111

volatile bool isRunning = true; // Flag für den Serverstatus

void *signalProcessingThread(void *arg)
{
    sigset_t *set = (sigset_t *)arg;
    int sig;

    while (isRunning) {
        // Warte auf ein Signal
        if (sigwait(set, &sig) == 0) {
            if (sig == SIGINT || sig == SIGTERM) {
                infoPrint("Received termination signal, cleaning up and exiting...");
                isRunning = false; // Setze das Flag auf false, um die Schleife zu beenden

                // Setze exitFlag und schließe den Server-Socket
                closeServerSocket();

                break; // Beende die Schleife
            }
        }
    }
    return NULL;
}


int main(int argc, char **argv)
{
    int port = DEFAULT_PORT;
    utilInit(argv[0]);
    infoPrint("Chat server, group 12"); 
	if (argc > 1) {
        port = atoi(argv[1]); // atoi converts string to integer
        if (port <= 1023 || port > 65535) // Check if port is in valid range
        {
            infoPrint("Invalid port number: %s. Using default port %d.", argv[1], DEFAULT_PORT);
            port = DEFAULT_PORT;
        } else {
            infoPrint("Using port: %d", port);
        }
    }
    else
    {
        infoPrint("Starting Server with default port: %d.", DEFAULT_PORT);
    }
    infoPrint(" 'Admin' is the default admin user");

    // Signal handler registrieren ---------------------------------------------
    sigset_t set;
    pthread_t signalThread;

    // Signalmaske erstellen
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    // Blockiere Signale im Hauptthread
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        errnoPrint("Failed to block signals in main thread");
        exit(EXIT_FAILURE);
    }

    // Starte den Signal-Verarbeitungsthread
    if (pthread_create(&signalThread, NULL, signalProcessingThread, (void *)&set) != 0) {
        errnoPrint("Failed to create signal processing thread");
        exit(EXIT_FAILURE);
    }
// ---------------------------------------------------------------------------

    
    const int broadcast = broadcastAgentInit();
    if (broadcast != 0) {
        errorPrint("Broadcast agent initialization failed");
        return EXIT_FAILURE;
    }

    // Starte connectionHandler in einem separaten Thread
    pthread_t connectionThread;
    if (pthread_create(&connectionThread, NULL, (void *(*)(void *))connectionHandler, (void *)(intptr_t)port) != 0) {
        errnoPrint("Failed to create connection handler thread");
        return EXIT_FAILURE;
    }

    // Warte auf den Signal-Verarbeitungsthread
    pthread_join(signalThread, NULL);

    // Beende den connectionHandler-Thread, falls er noch läuft
    pthread_cancel(connectionThread);
    pthread_join(connectionThread, NULL);

    infoPrint("Shutting down server...");
    broadcastAgentCleanup();
    destroyUserList();

    return EXIT_SUCCESS;
}