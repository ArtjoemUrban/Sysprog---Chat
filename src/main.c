#define _POSIX_C_SOURCE 199506L // Für POSIX sigwait
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h> // Für isdigit
#include <limits.h> // Für INT_MAX
#include <errno.h>


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

                closeServerSocket();
                broadcastAgentCleanup();
                destroyUserList();
                

                break; // Beende die Schleife
            }
        }
    }
    return NULL;
}


int main(int argc, char **argv)
{
   // debugEnable(); 
    int port = DEFAULT_PORT;
    utilInit(argv[0]);
    infoPrint("Chat server, group 12"); 
	if (argc > 1) {

        char *endptr; // Pointer für die Fehlerbehandlung
        errno = 0; 

        long parsedPort = strtol(argv[1], &endptr, 10); // strtol für robustere Fehlerbehandlung
        
        // Überprüfe, ob die Konvertierung erfolgreich war und ob der Port im gültigen Bereich liegt
        if( errno != 0 || *endptr != '\0' || parsedPort < 1024 || parsedPort > 65535) {
            errorPrint("Invalid port number: %s. Must be a number between 1024 and 65535", argv[1]);
            return EXIT_FAILURE;
        } else {
            port = (int)parsedPort;
            infoPrint("Using port: %d", port);
        } 
    }else {
        infoPrint("Using default port: %d", DEFAULT_PORT);
    }
    
    infoPrint(" 'Admin' is the default admin user");

    // Signal handler registrieren
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

    // Socket erstellen
    int serverSocket = createPassiveSocket(port);
    if (serverSocket == -1) {
        errnoPrint("Failed to create server socket");
        return EXIT_FAILURE;
    }
    
    // Broadcast-Agent initialisieren
    const int broadcast = broadcastAgentInit();
    if (broadcast != 0) {
        errorPrint("Broadcast agent initialization failed");
        return EXIT_FAILURE;
    }

    // Starte connectionHandler in einem separaten Thread
    pthread_t connectionThread;
    if (pthread_create(&connectionThread, NULL, (void *(*)(void *))connectionHandler, (void *)&serverSocket) != 0) {
        errnoPrint("Failed to create connection handler thread");
        return EXIT_FAILURE;
    }

    // Warte auf den Signal-Verarbeitungsthread
    pthread_join(signalThread, NULL);

    // Beende den connectionHandler-Thread, falls er noch läuft
    pthread_cancel(connectionThread);
    pthread_join(connectionThread, NULL);

    infoPrint("Shutting down server...");
    //debugDisable(); 

    return EXIT_SUCCESS;
}