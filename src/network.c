#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <endian.h>
#include <fcntl.h> // Für fcntl (file control)-> Socket Validierung

#include <stdbool.h>
#include "network.h"
#include "util.h"
#include "user.h"

bool reciveLoginRequest(int fd, LoginRequest * msg)
{
	// Header lesen
    ssize_t received = recv(fd, &msg->header, sizeof(Header), MSG_WAITALL);
    if (received !=sizeof(Header)) {
        errnoPrint("Fehler beim empfangen des LRQ");
        return false;
    }
    if ((size_t)received != sizeof(Header)) {
        errorPrint("Header hat falsche Länge");
        return false;
    }
    // Header-> Type prüfen
    if (msg->header.type != LRQ) {
        errorPrint("Erste Nachricht ist kein LRQ");
        return false;
    }
	// Header-> Len prüfen
	msg->header.len = ntohs(msg->header.len);
	if(msg->header.len < 5 || msg->header.len > (USERNAME_RAW_MAX +5))
	{
		errorPrint("Ungültige länge");
		return false;
	}
    // Restliche Daten lesen 
    received = recv(fd, ((char*)msg) + sizeof(Header), msg->header.len, MSG_WAITALL); // ((char*)buff) + sizeof(Header) position nach hader
    if (received != msg->header.len) {
        errorPrint("Restdaten haben falsche Länge");
        return false;
    }
    // Magic prüfen
	msg->magic = ntohl(msg->magic);
    if (msg->magic != MAGIC) {
        errorPrint("Falsche Magic");
        return false;
    }
    return true;
	//username prüfen
	if (msg->name[0] == '\0' || memchr(msg->name, '\0', USERNAME_RAW_MAX)) {
    errorPrint("Ungültiger Username (Null-Byte oder leer)");
    return false;
	}
};

void sendLoginResponse(int fd, uint8_t code)
{
	// Servername Prüfen
	const char *server_name = SERVER_NAME;
	size_t server_name_len = strlen(server_name);
	if(server_name_len < 1 || server_name_len > SNAME_MAX)
	{
		errorPrint("Servername ist ungültig für LRE");
		return;
	}

	LoginResponse loginResponse;
	// Header
	loginResponse.header.type = LRE;
	loginResponse.header.len = htons(4 + 1 + server_name_len);

	// Payload
	loginResponse.magic = htonl(0xc001c001);
	loginResponse.code = code; // 1 Byte keine umwandlung nötig

	memset(loginResponse.serverName, 0, SNAME_MAX); // damit keine Seltsamen Daten gesendet werden
	memcpy(loginResponse.serverName, server_name, server_name_len); // Name wird gesetzt

	// gesamt länge
	size_t loginResponse_len = sizeof(Header) + 4 + 1 + server_name_len;

	// SEnden
	size_t sent = send(fd, &loginResponse, loginResponse_len, 0);
	if(sent != (size_t) loginResponse_len)
	{
		errnoPrint("Fehler beim senden des LRE");
	}
};

// Benachrichtigt alle User über den neuen User
void sendUserAddedtoALL(User *user, void *arg) 
{
	const char *name = (const char *) arg;
	size_t name_len = strnlen(name, USERNAME_RAW_MAX); // sucht 31 bytes nach Null-Terminierung wird sie nicht gefunden wird länge USER_NAME_MAX
	if(name_len > USERNAME_RAW_MAX || name_len == 0)
	{
		errorPrint("Name hat ungueltige laenge: %zu", name_len);
		return;
	}

	// erstell UAD
	UserAdded message;
	memset(&message, 0, sizeof(UserAdded)); // setzt erst mal alles auf 0
	message.header.type = UAD;
	message.header.len = htons(8 + name_len);
	message.timestamp = htobe64((uint64_t)time(NULL)); // Aktueller Timestamp
	memcpy(message.name, name, name_len);

	size_t total_len = sizeof(Header) + 8 + name_len;
	ssize_t sent = send(user->sock, &message, total_len,0);
	if(sent != (ssize_t)total_len)
	{
		errnoPrint("konnte UserAdded nicht versenden an %s", user->name);
	}
}

// Sendet die Namen aller User an den neuen User
void sendUserListToNewUser(User *user, void *arg)
{
    int socket_fd = *(int *)arg;
	if(user->sock == socket_fd)
	{
		return; // nicht an den eigenen User senden
	}

    size_t name_len = strnlen(user->name, USERNAME_RAW_MAX);  // sicherstellen, dass kein '\0' innerhalb der Länge auftritt
    if (name_len == 0 || name_len > 31) {
        errorPrint("Ungültige Länge für Usernamen: %zu", name_len);
        return;
    }
    // Nachricht vorbereiten
    UserAdded message;
    memset(&message, 0, sizeof(UserAdded));  // Initialisiere alles mit 0

    message.header.type = UAD;
    message.header.len = htons(8 + name_len);  // Nur Timestamp 8 Bytes + Name-Länge
    message.timestamp = 0;  // 0 -> "User war schon da"

    memcpy(message.name, user->name, name_len);  // Nur gültige Bytes kopieren

    size_t total_len = sizeof(Header) + 8 + name_len;  // Gesamtgröße der Nachricht

    // Nachricht senden
    ssize_t sent = send(socket_fd, &message, total_len, 0);
    if (sent != (ssize_t)total_len) {
        errnoPrint("Konnte User: %s nicht ankündigen", user->name);
    }
}

UserRemoved createUserRemovedMessage(u_int8_t code, const char* name)
{
	size_t name_len = strnlen(name, USERNAME_RAW_MAX);
	UserRemoved message;
	message.header.type = 5;

	message.header.len = htons(sizeof(uint64_t) + sizeof(uint8_t) + name_len);
	message.timestamp = hton64u((uint64_t)time(NULL));
	message.code = code;

	memcpy(message.name, name, name_len);
	return message;
}

void sendUserRemoved(User *user, void *arg)
{
	const UserRemoved* message = (const UserRemoved*)arg;
	size_t total_len = sizeof(Header) + ntohs(message->header.len);

	/*char uuser[4] = "user";		
	if (memcmp(user->name, uuser, 4) == 0)
	{
		return;
	}*/

	ssize_t sent = send(user->sock, message, total_len, MSG_NOSIGNAL);
	if(sent != (ssize_t)total_len)
	{
		errnoPrint("Fehler beim senden der URM Message");
	}
}

int reciveC2S(int sock, Client2Server *msg)
{
	Header header;

	ssize_t recived = recv(sock, &header, sizeof(Header),0);
	if(recived == 0)
	{
		return 0; // Verbindung ordentlich beendet
	}else if (recived < 0)
	{
		return -1; // Verbindung abgebrochen
	}
	if(recived != sizeof(Header))
	{
		errnoPrint("konnte Nachricht vom User nicht empfangen");
		return 2;
	}
	if (header.type != C2S)
	{
		errorPrint("Falscher Type: %u, in C2S", header.type);
		return 2;
	}
	header.len = htons(header.len); // Host Byte Order
	if(header.len > MSG_MAX)
	{
		errorPrint(" C2S Nachricht ist zu Lang");
		return 2;
	}
	memset(msg,0,sizeof(Client2Server)); // setzt zuerst alles auf 0 
	msg->header = header;

	// empfange Bytes wie im len Feld 
	recived = recv(sock,&msg->text, header.len,0);
	if(recived != header.len)
	{
		errnoPrint("konnte Text nicht empfangen");
		return -1;
	}

	debugPrint("Empfangene Nachricht Type: %u Len: %u Text %20s...", msg->header.type, msg->header.len, msg->text);
	return 1;
}

void sendS2C(User *user, void *msg)
{
	Server2Client *s2c = (Server2Client *)msg;
	size_t total_len = sizeof(Header) + ntohs(s2c->header.len);
	if( total_len > 555)
	{
		errorPrint("Nachricht ist zu lang zum Versenden");
	}
	ssize_t sent = send(user->sock, s2c, total_len,0);
	if(sent != (ssize_t)total_len)
	{
		errorPrint("Fehler beim senden der S2C an %s", user->name);
	}
}

void createS2CMessage(Server2Client *msg, const char *sender, const char *text, size_t text_len)
{
	memset(msg, 0, sizeof(Server2Client)); // aufräumen

	msg->header.type = S2C;
	msg->timeStamp = hton64u((uint64_t)time(NULL));

	strncpy(msg->originalSender, sender, USERNAME_MAX); // Sender
	memcpy(msg->text, text, text_len);

	msg->header.len = htons(sizeof(uint64_t) + USERNAME_MAX + text_len);
}


void sendS2CError(int client_fd, const char *text)
 {
	Server2Client msg;
	createS2CMessage(&msg, "", text, strlen(text));

	// Sende die Nachricht an den Client
	ssize_t sent = send(client_fd, &msg, sizeof(msg.header) + ntohs(msg.header.len), 0);
	if (sent < 0) {
		errnoPrint("Fehler beim Senden der S2C Error Nachricht an Client %d", client_fd);
	}
 }
