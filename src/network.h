#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MSG_MAX 512  // Maximale Länge des Textteils gemäß RFC
#define USERNAME_MAX 32 //inkl. nullterminiert für S2C
#define USERNAME_RAW_MAX 31 //max. Länge von Namen ohne Null-Terminator


/* TODO: When implementing the fully-featured network protocol (including
 * login), replace this with message structures derived from the network
 * protocol (RFC) as found in the moodle course. */

 #define MAGIC 0x0badf00d
 #define VERSION 0
 #define USER_NAME_MAX 32
 #define SNAME_MAX 31

 typedef enum
 {
	LRQ = 0,    // Login Request
	LRE = 1,	// Login Response
	C2S = 2,	// Client to Server
	S2C = 3,	// Server to Client
	UAD = 4,	// User Added
	URM = 5, 	// User Removed
 } MessageType;

 
 //Header für alle Nachrichtentypen
 typedef struct __attribute__((packed))
 {
	uint8_t type;
	uint16_t len;		//real length of the text (big endian, len <= MSG_MAX)
 } Header;
 
 // LRQ
 typedef struct  __attribute__((packed))
 {
   Header header;
   uint32_t magic;
   uint8_t version; // ist zurzeit 0
   char name[USER_NAME_MAX];
 } LoginRequest;

 bool reciveLoginRequest(int fd, LoginRequest *buff);
 /* The LoginRequest message always has to be the first message that is sent from
the client to the server. If the server detects an invalid type, length or
magic, it shall close the connection without sending any message back.
In any other case, it shall send back a LoginResponse message, as given below.
Names may contain every ASCII character with a value between (including) 33
and 126, except for the quote ('), the double quote ("), and the backtick (`).
If the Name field contains an invalid byte, the server shall notify the client
by setting the Code in the LoginResponse accordingly.
*/

typedef enum 
{
	SUCCESS = 0,
	NAME_TAKEN = 1,
	NAME_INVALID = 2,
	PROTOCOL_VERSION_MISMATCH = 3,
	SERVER_ERROR = 255,
}LREStatusCode; // für LRE

typedef struct  __attribute__((packed))
{
  Header header;
  uint32_t magic;
  uint8_t code;
  char serverName[SNAME_MAX];
  
} LoginResponse;

void sendLoginResponse(int fd, uint8_t code);
/* The LoginResponse message always is the first message the client receives from
the server. The log in only was successful, if the LoginRequest has the
correct Type, a valid Length, the correct Magic and Code=0. In any other case,
the connection shall be closed by the client and the server.
The rules for allowed server names are the same as for user names.
If the client detects an invalid server name, it shall drop the connection to
the server immediately.*/


 typedef struct __attribute__((packed))
 {
	Header header;
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
	 Header header;
	 uint64_t timestamp;
	 char name[USERNAME_RAW_MAX];  // not null-terminated
 } UserAdded;

typedef enum 
{
	LEFT = 0,
	KICKED = 1,
	ERROR = 2
} RemoveReson;
 
typedef struct __attribute__((packed))
{
    Header header;
    uint64_t timestamp;
    uint8_t code;  // 0: left, 1: kicked, 2: error
    char name[USERNAME_RAW_MAX];  // not null-terminated
} UserRemoved;






 /* simple-client: */
typedef struct __attribute__((packed))
{
	uint16_t len;		//real length of the text (big endian, len <= MSG_MAX)
	char text[MSG_MAX];	//text message
} Message;

int networkReceive(int fd, Message *buffer);
int networkSend(int fd, const Message *buffer);

#endif 
