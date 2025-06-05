#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "user.h"
#include "clientthread.h"
#include "util.h"

static pthread_mutex_t userLock = PTHREAD_MUTEX_INITIALIZER;
static User *userFront = NULL;
static User *userBack = NULL;

//TODO: Implement the functions declared in user.h
User *add_user(int sock)
{
    User *newUser = malloc(sizeof(User));
    if (!newUser) {
    errnoPrint("malloc failed for creating user");
    return NULL;
    }
    newUser->sock = sock;
    newUser->prev = NULL;
    newUser->next = NULL;

    pthread_mutex_lock(&userLock); // blokiert die liste für andere Threads damit die liste nicht beschädigt wird
    if (!userFront) {
        userFront = userBack = newUser;
    } else {
        userBack->next = newUser;
        newUser->prev = userBack;
        userBack = newUser;
    }
    pthread_mutex_unlock(&userLock); // Gibt die liste wieder frei

    if (pthread_create(&newUser->thread, NULL, clientthread, newUser) != 0) {
        errnoPrint("failed to create user-thread");
        remove_user(newUser);
        return NULL;
    }

    infoPrint("Neuer Benutzer + Thread erstellt und LRQ wird verarbeitet");
    return newUser;
}

void remove_user(User *user)
{
    if(!user) return;
    pthread_mutex_lock(&userLock);
    if (user->prev) user->prev->next = user->next;
    else userFront = user->next;

    if (user->next) user->next->prev = user->prev;
    else userBack = user->prev;

    pthread_mutex_unlock(&userLock);
    close(user->sock); // schließt socket
    free(user);
    infoPrint("User removed");
}

void iterate_users(void (*callback)(User *, void *), void *arg)
{
    pthread_mutex_lock(&userLock);
    User *current = userFront;
    while (current) 
    {
        User *next = current->next;
        callback(current, arg);
        current = next;
    }
    pthread_mutex_unlock(&userLock);
}

bool isNameTaken(const char* newName)
{
    pthread_mutex_lock(&userLock);
    User *current = userFront;
    while(current)
    {
        if(strcmp(current->name, newName) == 0)
        {
            pthread_mutex_unlock(&userLock);
            return true;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&userLock);
    return false;
}