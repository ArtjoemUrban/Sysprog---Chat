#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "network.h"
#include "util.h"

int networkReceive(int fd, Message *buffer)  //gibt bei erfolg 0 zurück und -1 bei fehler
{
	uint16_t net_len;
	// Receive length
	ssize_t received = recv(fd, &net_len, sizeof(net_len), MSG_WAITALL);  // MSG_WAITALL -> recv() wird solange warten bis alle bytes übertragen sind
																	      // sonst könnte recv() zu wenig bytes aus dem eingangs puffer lesen 
																	      // ausname verbindung bricht ab
	if(received != sizeof(net_len))
	{
		errorPrint("length field passt nicht zum protokoll ( recv returned %zd): %s", received, strerror(errno));
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
