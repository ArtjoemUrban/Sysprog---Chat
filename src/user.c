#include <pthread.h>
#include "user.h"
#include "clientthread.h"

static pthread_mutex_t userLock = PTHREAD_MUTEX_INITIALIZER;
static User *userFront = NULL;
static User *userBack = NULL;

//TODO: Implement the functions declared in user.h


//beim löschen des USER nicht vergessen beim beebden des thread free() für die adresse von client_fd (ist die socket nummer)