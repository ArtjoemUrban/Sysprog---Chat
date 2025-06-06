#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <endian.h>

#include <stdbool.h>
#include "network.h"
#include "util.h"
#include "user.h"

bool chat_paused = false;

typedef struct {
    int fd;
    char name[USER_NAME_MAX];
} Client;

#define MAX_CLIENTS 100
Client clients[MAX_CLIENTS];
int client_count = 0;

bool reciveLoginRequest(int fd, LoginRequest * buff)
{
	// Header lesen
    ssize_t received = recv(fd, &buff->header, sizeof(Header), MSG_WAITALL);
    if (received !=sizeof(Header)) {
        errnoPrint("Fehler beim empfangen des LRQ");
		
        return false;
    }
    if ((size_t)received != sizeof(Header)) {
        errorPrint("Header hat falsche Länge");
        return false;
    }

    // Header-> Type prüfen
    if (buff->header.type != LRQ) {
        debugPrint("Erste Nachricht ist kein LRQ");
        return false;
    }
	
	// Header-> Len prüfen
	buff->header.len = ntohs(buff->header.len);
	if(buff->header.len < 5 || buff->header.len > (USER_NAME_MAX +5))
	{
		errorPrint("Ungültige länge");
		return false;
	}

    // Restliche Daten lesen 
    received = recv(fd, ((char*)buff) + sizeof(Header), buff->header.len, MSG_WAITALL); // ((char*)buff) + sizeof(Header) position nach hader
    if (received != buff->header.len) {
        errorPrint("Restdaten haben falsche Länge");
        return false;
    }
    // Magic prüfen
	buff->magic = ntohl(buff->magic);
    if (buff->magic != MAGIC) {
        errorPrint("Falsche Magic");
        return false;
    }
    return true;

	//username prüfen
	if (buff->name[0] == '\0' || memchr(buff->name, '\0', USERNAME_RAW_MAX)) {
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

	// Nachricht muss erstellt werden
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
	if(sent != (ssize_t) loginResponse_len)
	{
		errnoPrint("Fehler beim senden des LRE");
	}

};

void sendUserAddedtoALL(User *user, void *arg) 
{
	const char *name = (const char *) arg;
	size_t name_len = strnlen(name, USER_NAME_MAX); // sucht 31 bytes nach Null-Terminierung wird sie nicht gefunden wird länge USER_NAME_MAX
	if(name_len > USER_NAME_MAX || name_len == 0)
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
	infoPrint("UserAdded Nachricht an %s gesendet", user->name);
}

void sendUserListToNewUser(User *user, void *arg)
{
    int socket_fd = *(int *)arg;

    size_t name_len = strnlen(user->name, USER_NAME_MAX);  // sicherstellen, dass kein '\0' innerhalb der Länge auftritt
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
	size_t name_len = strnlen(name, USER_NAME_MAX);

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
	infoPrint("sending URM: type %u, code %u, ",message->header.type,message->code);
	size_t total_len = sizeof(Header) + ntohs(message->header.len);
	ssize_t sent = send(user->sock, message, total_len, 0);
	if(sent != (ssize_t)total_len)
	{
		errnoPrint("Fehler beime senden der URM Message");
	}

}




//----------------------------------Aufgabe 1 Simple Client --------------------------------------------------
 int networkReceive(int fd, Message *buffer)  //gibt bei erfolg 0 zurück und -1 bei fehler
{
	uint16_t net_len;
	// Receive length
	ssize_t received = recv(fd, &net_len, sizeof(net_len), MSG_WAITALL);  // MSG_WAITALL -> recv() wird solange warten bis alle bytes übertragen sind
																	      // sonst könnte recv() zu wenig bytes aus dem eingangs puffer lesen 
																	      // ausname verbindung bricht ab
	if(received != sizeof(net_len))
	{
		debugPrint("length field passt nicht zum protokoll ( recv returned %zd)", received);
		return -1;
	}

	// Convert length byte order
	buffer->len = ntohs(net_len); // Konvertiert NetzwerkByte-Order zu Host-Byte-Order

	// Validate length
	if(buffer->len > MSG_MAX)
	{
		errorPrint("nachricht zu lang");
		return -1;
	}
	// Receive text
	received = recv(fd, buffer->text,buffer->len, MSG_WAITALL);  
	if(received != buffer->len) // prüfung ob nachricht zum längenfeld passt
	{
		errorPrint("Nachricht hat nicht selbe laenge wie length field es vorschreibt");
		return -1;
	}

	buffer->text[buffer->len] = '\0'; // null terminierung der Nachricht


	return 0;
}

int networkSend(int fd, const Message *buffer)  //gibt bei erfolg 0 zurück und -1 bei fehler
{
	// Send complete message
	if(buffer->len > MSG_MAX)  // solte passen da dies schon beim empfangen der Nachricht geprüft wird
	{
		errorPrint("Nachricht zu lang");
		return -1;
	}

	// Convert length byte order
	uint16_t msg_len = htons(buffer->len); // setzt Host-Byte-Order auf Network-Byte-Order

	// -> auf big Endian da dies von TCP erwartet wird

	if(send(fd,&msg_len, sizeof(msg_len),0) != sizeof(msg_len))  // sendet die länge
	{
		return -1;
	}

	if(send(fd, buffer->text, buffer->len, 0) != buffer->len)  // sendet nachricht
	{
		return -1;
	}

	return 0;
}

/*int broadcastMsg(const void* msg, size_t msgSize)
{

}*/

// Sendet eine Textnachricht vom Client zum Server
/*void send_c2s_message(int socket, const char *message) {
    uint8_t type = 2;
    uint16_t length = strlen(message); // <= 512 prüfen
    if (length > 512) {
        fprintf(stderr, "Message too long\n");
        return;
    }

    uint8_t header[3];
    header[0] = type;
    header[1] = (length >> 8) & 0xFF;
    header[2] = length & 0xFF;

    send(socket, header, 3, 0);
    send(socket, message, length, 0);
}

void handle_c2s_message(int client_fd, const char *message, size_t length) {
    if (message[0] == '/') {
        // Command (z.B. /pause)
        if (strcmp(message, "/pause") == 0) {
            broadcast_server_message("Chat wurde pausiert.");
            chat_paused = true;
        } else {
            send_s2c_error(client_fd, "Unbekannter Befehl");
        }
    } else {
        // Reguläre Nachricht weiterleiten
        time_t timestamp = time(NULL);
        const char *sender_name = get_username_by_fd(client_fd);
        if (!sender_name) return;
        broadcast_s2c_message(sender_name, message, timestamp);
    }
}

void broadcast_s2c_message(const char *sender, const char *text, time_t timestamp) {
    uint8_t type = 3;
    uint32_t len_sender = strlen(sender) + 1; // null-terminated
    uint16_t length = 8 + 32 + strlen(text);  // timestamp + sender + text
    if (length > 552) length = 552;

    uint8_t header[3];
    header[0] = type;
    header[1] = (length >> 8) & 0xFF;
    header[2] = length & 0xFF;

  	for (int i = 0; i < client_count; ++i) {
    Client *c = &clients[i];
        send(c->fd, header, 3, 0);
        send_u64(c->fd, (uint64_t)timestamp);

        char sender_field[32] = {0};
        strncpy(sender_field, sender, 31);
        send(c->fd, sender_field, 32, 0);

        send(c->fd, text, strlen(text), 0);
    }
}

void send_s2c_error(int client_fd, const char *text) {
    uint8_t type = 3;
    uint16_t length = 8 + 32 + strlen(text); // timestamp + sender + text
    if (length > 552) length = 552;

    uint8_t header[3];
    header[0] = type;
    header[1] = (length >> 8) & 0xFF;
    header[2] = length & 0xFF;

    uint64_t timestamp = (uint64_t)time(NULL);
    char sender_field[32] = {0}; // leer, d.h. Servernachricht

    send(client_fd, header, 3, 0);
    send_u64(client_fd, timestamp);
    send(client_fd, sender_field, 32, 0);
    send(client_fd, text, strlen(text), 0);
} */