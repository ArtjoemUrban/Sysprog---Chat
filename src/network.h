#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H

#include <stdint.h>

/* TODO: When implementing the fully-featured network protocol (including
 * login), replace this with message structures derived from the network
 * protocol (RFC) as found in the moodle course. */

 typedef enum
 {
	LRQ = 0,    // Login Request
	LRE = 1,	// Login Response
	C2S = 2,	// Client to Server
	S2C = 3,	// Server to Client
	UAD = 4,	// User Added
	URM = 5, 	// User Removed
 } MessageType;

 typedef struct __attribute__((packed))
 {
	uint8_t type;
	uint16_t len;		//real length of the text (big endian, len <= MSG_MAX)
 } Header;
 
 typedef struct  __attribute__((packed))
 {
	//struct Header h;
 } MessageP;
 



 /* simple-client: */
enum { MSG_MAX = 1024 };
typedef struct __attribute__((packed))
{
	uint16_t len;		//real length of the text (big endian, len <= MSG_MAX)
	char text[MSG_MAX];	//text message
} Message;

int networkReceive(int fd, Message *buffer);
int networkSend(int fd, const Message *buffer);

#endif
