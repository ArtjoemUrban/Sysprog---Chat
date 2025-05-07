#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "user.h"
#include "clientthread.h"

static pthread_mutex_t userLock = PTHREAD_MUTEX_INITIALIZER;
static User *userFront = NULL;
static User *userBack = NULL;

//TODO: Implement the functions declared in user.h
User *add_user(int sock)
{
User *newUser = malloc(sizeof(User));
if (!newUser) {
perror("malloc failed");
return NULL;
}
newUser->sock = sock;
newUser->prev = NULL;
newUser->next = NULL;

pthread_mutex_lock(&userLock);
if (!userFront) {
    userFront = userBack = newUser;
} else {
    userBack->next = newUser;
    newUser->prev = userBack;
    userBack = newUser;
}
pthread_mutex_unlock(&userLock);

if (pthread_create(&newUser->thread, NULL, clientthread, newUser) != 0) {
    perror("pthread_create failed");
    remove_user(newUser);
    return NULL;
}

return newUser;

}

void remove_user(User *user)
{
pthread_mutex_lock(&userLock);
if (user->prev) user->prev->next = user->next;
else userFront = user->next;

if (user->next) user->next->prev = user->prev;
else userBack = user->prev;

pthread_mutex_unlock(&userLock);
close(user->sock);
free(user);

}

void iterate_users(void (*callback)(User *, void *), void *arg)
{
pthread_mutex_lock(&userLock);
User *current = userFront;
while (current) {
User *next = current->next;
callback(current, arg);
current = next;
}
pthread_mutex_unlock(&userLock);
}