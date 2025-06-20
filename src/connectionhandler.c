#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h> // close()
#include <netinet/in.h> // sockaddr_in : vereinfacht die erzeugung von sockets
#include <pthread.h> // threads erzeugen
#include <string.h>
#include <stdlib.h>
#include "connectionhandler.h"
#include "util.h"
#include "clientthread.h" // wird benötigt um Client thread zu erzeugen
#include "user.h" // benötigt zum erstellen eines Users

static int serverSocket = -1;
static volatile int exitFlag = 0; 

void closeServerSocket(void)
{
	close(serverSocket);
	infoPrint("Server socket closed.");
	exitFlag = 1; 
	
}

int createPassiveSocket(in_port_t port)
{
	int fd = -1;
	fd = socket(AF_INET, SOCK_STREAM, 0); // AF_INET = IPv4, SOCKE_STREAM = TCP, 0 = Protokoll (üblicherweise)
	if (fd == -1) 
	{
		errnoPrint("Could not create Socket");
		return -1; // falls erstellung nicht Funktioniert
	}

    int opt = 1;
    // Setze den Socket-Option SO_REUSEADDR, um den Socket wiederverwenden zu können
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
    {
        close(fd);
        errnoPrint("Could not set socket options");
        return -1;
    }

	struct sockaddr_in s_addr;  // erstellt struct für socket-addresse
	memset(&s_addr, 0, sizeof(s_addr));
	s_addr.sin_family = AF_INET;  // IPv4
	s_addr.sin_addr.s_addr = INADDR_ANY; // akzeptiert verbindungen auf allen interfaces
	s_addr.sin_port = htons(port);  // setzt den Port, htons() wandelt die Byte-Reihenfolge um
	
	// Bindet den Socket an die Adresse
	if (bind(fd,(struct sockaddr*)&s_addr, sizeof(s_addr)) == -1)  // s_addr muss mit dem socket fd kommpatibel sein
	{
		close(fd); 
		errnoPrint("Could not bind socket to address");
		return -1;
	};

	// Markiert den Socket als passiven Socket, der auf eingehende Verbindungen wartet
	if (listen(fd, 4) == -1) // SOMAXCONN kann als zweiter parameter für Warteschlange verwendet werden
	{
		close(fd);
		errnoPrint("Could not mark socket to be accepting conections");
		return -1;
	}

	return fd;
}

void *connectionHandler(void *arg)
{
    const int fd = *(int*)(intptr_t)arg; // Castet das Argument zu einem int, das den Socket repräsentiert
    serverSocket = fd; // Speichert den Socket in der globalen Variable

    while (exitFlag == 0) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int* client_fd = malloc(sizeof(int));
        if (client_fd == NULL) {
            errnoPrint("Memory allocation failed");
            continue;
        }

        *client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_len);
        if (*client_fd == -1) {
            if (exitFlag != 0) {
                // Schleife wurde durch Signal unterbrochen
                free(client_fd);
                break;
            }
            errnoPrint("Could not accept Client Connection");
            free(client_fd);
            continue;
        }

        if (add_user(*client_fd) == NULL) {
            errnoPrint("Konnte keinen User erzeugen");
            close(*client_fd);
            free(client_fd);
            continue;
        }
    }

    close(fd);
    return NULL;
}


