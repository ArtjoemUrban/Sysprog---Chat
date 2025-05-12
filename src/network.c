#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "network.h"
#include "util.h"

int networkReceive(int fd, Message *buffer)
{
	uint16_t net_len;
	//TODO: Receive length
	ssize_t recived = recv(fd, &net_len, sizeof(net_len), MSG_WAITALL);
	if(recived != sizeof(net_len))
	{
		errorPrint("length field passt nicht zum protokoll");
		return -1;
	}

	//TODO: Convert length byte order
	buffer->len = ntohs(net_len); // wandelt erhaltene bit order auf littleEndian um, damit system sie verarbeiten kann

	//TODO: Validate length
	if(buffer->len > MSG_MAX)
	{
		errorPrint("nachricht zu lang");
		return -1;
	}
	//TODO: Receive text
	recived = recv(fd, buffer->text,buffer->len, MSG_WAITALL);
	if(recived != buffer->len) // prüfung ob nachricht zum längenfeld passt
	{
		errorPrint("Nachricht hat nicht selbe laenge wie length field es vorschreibt");
		return -1;
	}

	buffer->text[buffer->len] = '\0'; // null terminierung der Nachricht


	return 0;
}

int networkSend(int fd, const Message *buffer)
{
	//TODO: Send complete message
	if(buffer->len > MSG_MAX)  // solte passen da dies schon beim empfangen der Nachricht geprüft wird
	{
		errorPrint("Nachricht zu lang");
		return -1;
	}

	uint16_t msg_len = htons(buffer->len); // setzt die korrekte byte order für kommunikation über das Netzwerk 

	// -> auf big Endian da dies von TCP erwartet wird

	if(send(fd,&msg_len, sizeof(msg_len),0) != sizeof(msg_len))  // sendet die länge
	{
		return -1;
	}

	if(send(fd, buffer->text, buffer->len, 0) != buffer->len)
	{
		return -1;
	}

	return 0;
}
