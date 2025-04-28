#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h> // cloes()
#include <netinet/in.h> // sockaddr_in : vereinfacht die erzeugung von sockets
#include <pthread.h> // threads erzeugen
#include "connectionhandler.h"
#include "util.h"
#include "clientthread.h" // wird benötigt um Client thread zu erzeugen
#include "user.h" // benötigt zum erstellen eines Users


static int createPassiveSocket(in_port_t port)
{
	int fd = -1;
	//TODO: socket()
	fd = socket(AF_INET, SOCK_STREAM, 0); // AF_INET = IPv4, SOCKE_STREAM = TCP, 0 = Protokoll (üblicherweise)
	if (fd == -1) 
	{
		perror("Could not create Socket");
		return -1; // falls erstellung nicht Funktioniert
	}

	struct sockaddr_in s_addr;  // erstellt struct für socket-addresse
	memset(&s_addr, 0, sizeof(s_addr));
	s_addr.sin_family = AF_INET;  // IPv4
	s_addr.sin_addr.s_addr = INADDR_ANY; // akzeptiert verbindungen auf allen interfaces
	s_addr.sin_port = htons(port);  // gegebener Port bsp(htons(80) = Http)
	
	
	//TODO: bind() to port
	if (bind(fd,(struct sockaddr*)&s_addr, sizeof(s_addr)) == -1)  // s_addr muss mit dem socket fd kommpatibel sein
	{
		close(fd); 
		perror("Could not bind socket to address");
		return -1;
	};

	//TODO: listen()
	if (listen(fd, 4) == -1) // SOMAXCONN kann als zweiter parameter für Warteschlange verwendet werden
	{
		close(fd);
		perror("Could not mark socket to be accepting conections");
		return -1;
	}

	errno = ENOSYS;
	return fd;
}

int connectionHandler(in_port_t port)
{
	const int fd = createPassiveSocket(port);
	if(fd == -1)
	{
		errnoPrint("Unable to create server socket");
		return -1;
	}

	for(;;)
	{
		struct sockaddr_in client_addr; // erstellt address struct für client
		socklen_t client_len = sizeof(client_addr);

		int* client_fd = malloc(sizeof(int)); // speicher für jeden Thread reservieren , gespeichert wir
		if ( client_fd == 0)
		{
			errnoPrint("Memmory allocation failed");
			continue;
		}

		//TODO: accept() incoming connection
		*client_fd= accept(fd, (struct sockaddr*)&client_addr, &client_len);
		if ( client_fd == -1)
		{
			errnoPrint("Could not accept Client Connection");
			free(client_fd);
			continue;
		}

		//TODO: add connection to user list and start client thread
		//pthread_t thread = clientthread(client_fd); 
		pthread_t tid;  
		if ( pthread_create(&tid, NULL, clientthread, client_fd) != 0) // erzeugt thread
		{
			errnoPrint("Could not create Thread");
			close(*client_fd);
			free(client_fd);
			continue;
		}


		//createUser(tid, *client_fd);  -> erstellt User
	}

	return 0;	//never reached
}
