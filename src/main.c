#define _POSIX_C_SOURCE 199506L // Für POSIX sigwait 
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#include "connectionhandler.h"
#include "util.h"
#include "broadcastagent.h"
#include "user.h"

#define DEFAULT_PORT 8111

//volatile bool isRunning = true; // Flag für den Serverstatus

void *signalProcessingThread(void *arg)
{
    sigset_t *set = (sigset_t *)arg; 
    int sig;

        // Warte auf ein Signal
        if (sigwait(set, &sig) == 0) {
            if (sig == SIGINT || sig == SIGTERM) {
                debugPrint("Received termination signal, cleaning up and exiting...");
                //isRunning = false; // Setze das Flag auf false, um die Schleife zu beenden

                closeServerSocket();
                broadcastAgentCleanup();
                destroyUserList();   
            }
        }
    return NULL;
}


int main(int argc, char **argv) // argc = Anzahl der Argumente, argv = Array der Argumente
{
   // debugEnable(); 
    int port = DEFAULT_PORT;
    utilInit(argv[0]); // utilInit initialisiert die Hilfsfunktionen und setzt Programmname
    infoPrint("Chat server, group 12"); 
	if (argc > 1) {

        char *endptr; // Pointer für die Fehlerbehandlung
        errno = 0; 

        long parsedPort = strtol(argv[1], &endptr, 10); // strtol= String to long  besser als atoi
        
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

    // Signal handler registrieren
    sigset_t set; // Set für die Signalmaske
     

    // Signalmaske erstellen 
    sigemptyset(&set); // Leere Signalmaske initialisieren
    sigaddset(&set, SIGINT); // SIGINT hinzufügen (Strg+C)
    sigaddset(&set, SIGTERM); // SIGTERM hinzufügen (kill-Befehl)

    // Blockiere Signale im Hauptthread
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        errnoPrint("Failed to block signals in main thread");
        exit(EXIT_FAILURE);
    }
    pthread_t signalThread;
    // Starte den Signal-Verarbeitungsthread
    if (pthread_create(&signalThread, NULL, signalProcessingThread, (void *)&set) != 0) {
        errnoPrint("Failed to create signal processing thread");
        exit(EXIT_FAILURE);
    }

    // Socket erstellen
    int serverSocket = createPassiveSocket(port);
    if (serverSocket == -1) {
        errnoPrint("Failed to create server socket");
        exit(EXIT_FAILURE);
    }
    
    // Broadcast-Agent initialisieren
    const int broadcast = broadcastAgentInit();
    if (broadcast != 0) {
        errorPrint("Broadcast agent initialization failed");
        exit(EXIT_FAILURE);
        }

    // Starte connectionHandler in einem separaten Thread
    pthread_t connectionThread;
    if (pthread_create(&connectionThread, NULL, (void *(*)(void *))connectionHandler, (void *)&serverSocket) != 0) {
        errnoPrint("Failed to create connection handler thread");
        exit(EXIT_FAILURE);
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