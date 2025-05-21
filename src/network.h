#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define MSG_MAX 512  // Maximale Länge des Textteils gemäß RFC
#define USERNAME_MAX 32 //inkl. nullterminiert für S2C
#define USERNAME_RAW_MAX 31 //max. Länge von Namen ohne Null-Terminator


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

 typedef struct __attribute__((packed))
 {
	uint8_t type;
	uint16_t len;
	char text[MSG_MAX];

 }Client2Server;



 typedef struct __attribute__((packed))
 {
	uint8_t type;
	uint16_t len;
	uint64_t timeStamp;
	char originalSender[USERNAME_MAX];
	char text[MSG_MAX];

 }Server2Client;
 

 typedef struct __attribute__((packed))
 {
	 uint8_t type;
	 uint16_t len;
	 uint64_t timestamp;
	 char name[USERNAME_RAW_MAX];  // not null-terminated
 } UserAdded;

 typedef struct __attribute__((packed))
{
    uint8_t type;
    uint16_t len;
    uint64_t timestamp;
    uint8_t code;  // 0: left, 1: kicked, 2: error
    char name[USERNAME_RAW_MAX];  // not null-terminated
} UserRemoved;

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
