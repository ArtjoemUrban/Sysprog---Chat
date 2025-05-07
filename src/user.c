#include <pthread.h>
#include <assert.h>
#include "user.h"
#include <errno.h>
#include "clientthread.h"

static pthread_mutex_t userLock = PTHREAD_MUTEX_INITIALIZER;
static User *userFront = NULL;
static User *userBack = NULL;

//TODO: Implement the functions declared in user.h
User *listADD(pthread_t *thread, int client_sock)
{
    

    if(listFind(thread)!= NULL) //prüfen ob name bereits vorhanden
        errno = EEXIST;
        return NULL;

        User *newNode = malloc(sizeof(User)); //speicherplatz für neuen Eintrag vom Heap reservieren 
    if(newNode == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }   
    strcpy(newNode->name, name);  //werte für name in neues objekt   , sizeof(newNode->name)
       //null string für telefonnummer fürs erste


    newNode->prev = userBack;
    newNode->next = NULL;

    if(userFront = NULL)
        
    userFront = newNode; 
    else
        userBack->next=newNode;

    userBack = newNode;

    return newNode; 
}

void listForEach(void(*func)(User*))   //führt  für jedes listen element eine function aus // parameter der zu aufrufenden funktion muss ListNode* sein !!!
{
    assert(func != NULL);

    User *next;
    for(User *i = userFront; i != NULL; i = i->next )
    {
        next = i->next;
        func(i);
    }  //{ } Klammern müssen nach einem if-statement gesetzt werden, wenn mehrere Anweisungen folgen

}

int listRemoveByName(const char* name)
{
    assert(name != NULL);

    User *node = listFind(name);

    if(node == NULL)
    {
        errno = ENOENT;
        return -1;  //-1 für feheler (in dieser Aufgabe)
    }

    listRemove(node);
    return 0;  //0 für erfolg
}