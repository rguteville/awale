#ifndef SOCIAL_H
#define SOCIAL_H

/* Amis et mode privé */

#include "server.h"

int estAmi(Serveur *s, int i, const char *pseudo); /* 1 si pseudo est dans la liste d'amis du client i */

void cmdFriend(Serveur *s, int i, char *args);
void cmdUnfriend(Serveur *s, int i, char *args);
void cmdFriends(Serveur *s, int i, char *args);
void cmdPrivate(Serveur *s, int i, char *args);

#endif
