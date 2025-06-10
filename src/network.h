#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H

#include "user.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>



 #define MSG_MAX 512  // Maximale Länge des Textteils gemäß RFC
 #define USERNAME_MAX 32 //inkl. nullterminiert für S2C
 #define USERNAME_RAW_MAX 31 //max. Länge von Namen ohne Null-Terminator

 #define SERVER_NAME "Chat"
 #define SNAME_MAX 31 // server name nicht null terminiert

 #define MAGIC 0x0badf00d
 #define VERSION 0
 
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
   char name[USERNAME_RAW_MAX]; // !!! wird nicht nullterminiert daher 31 byte
 } LoginRequest;

 bool reciveLoginRequest(int fd, LoginRequest *buff);

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

typedef struct __attribute__((packed))
 {
	Header header;
	char text[MSG_MAX];
 }Client2Server;

 // -1 -> Verbindungs abbruch, 0 -> Verbindung beendet, 2 -> Fehler, 1 -> ok
 int reciveC2S(int sock, Client2Server *msg);

typedef struct __attribute__((packed))
 {
	Header header;
	uint64_t timeStamp;
	char originalSender[USERNAME_MAX]; // Nullterminiert -> 32 Byte (muss 32Byte sein)
	char text[MSG_MAX];

 }Server2Client;
 
 void handleS2C(const char *sender, const char *text, size_t text_len);
 void sendS2C(User *user, void *msg);

 typedef struct __attribute__((packed))
 {
	 Header header;
	 uint64_t timestamp;
	 char name[USERNAME_RAW_MAX];  // not null-terminated
 } UserAdded;

void sendUserAddedtoALL(User *user, void *arg); // User der die Nachricht erhält // arg ist ein char* auf den Namen des neuen Users

void sendUserListToNewUser(User *user, void *arg); // arg ist ein int* auf den socket des neuen users

typedef enum 
{
	LEFT = 0,
	KICKED = 1,
	ERROR = 2
} RemoveReason;
 
typedef struct __attribute__((packed))
{
    Header header;
    uint64_t timestamp;
    uint8_t code;  // 0: left, 1: kicked, 2: error
    char name[USERNAME_RAW_MAX];  // not null-terminated
} UserRemoved;

UserRemoved createUserRemovedMessage(uint8_t code, const char* name);
void sendUserRemoved(User *user, void *arg);





 /* simple-client: */
typedef struct __attribute__((packed))
{
	uint16_t len;		//real length of the text (big endian, len <= MSG_MAX)
	char text[MSG_MAX];	//text message
} Message;

int networkReceive(int fd, Message *buffer);
int networkSend(int fd, const Message *buffer);

#endif
