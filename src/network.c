#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <endian.h>
#include <stdbool.h>
#include "network.h"
#include "util.h"

bool reciveLoginRequest(int fd, LoginRequest * buff)
{
	// Header lesen
    ssize_t received = recv(fd, &buff->header, sizeof(Header), MSG_WAITALL);
    if (received !=sizeof(Header)) {
        errorPrint("recv(header)");
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
};

void sendLoginResponse(int fd, uint8_t code)
{
	LoginResponse loginResponse;
	// Nachricht muss erstellt werden

	if(send(fd, &loginResponse,sizeof(loginResponse)) != sizeof(loginResponse)) // evtl Header und payload getrent senden
	{
		return -1;
	}
};

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

// prototyp vlt falsch
UserRemoved create_remove_msg(const char* name, RemoveReson reson)
{
	UserRemoved msg;
	msg.header.type = URM;
	msg.header.len = htons(sizeof(UserRemoved));
	// msg.timestamp = htobe64(current_timestamp()); haben keine funktion für timestemp -> compiler fehler
	msg.code = reson;
	memcpy(msg.name, name,USERNAME_RAW_MAX);

	return msg;
}