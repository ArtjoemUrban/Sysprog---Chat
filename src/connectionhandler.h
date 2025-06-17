#ifndef CONNECTIONHANDLER_H
#define CONNECTIONHANDLER_H

#include <netinet/in.h> // in_port_t

/* 
-Erstellt einen Passiven Socket.
-Es wird für akzeptierte Verbindungen ein Socket erstellt
-Für jede verbindung wird ein Thread gestartet
*/

void *connectionHandler(void *arg);
void closeServerSocket(void);

#endif
