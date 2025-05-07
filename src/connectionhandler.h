#ifndef CONNECTIONHANDLER_H
#define CONNECTIONHANDLER_H

#include <netinet/in.h>

/* 
-Erstellt einen Passiven Socket.
-Es wird für akzeptierte Verbindungen ein Socket erstellt
-Für jede verbindung wird ein Thread gestartet
*/

int connectionHandler(in_port_t port);

#endif
