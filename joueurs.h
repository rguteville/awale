#ifndef JOUEURS_H
#define JOUEURS_H

/* Connexion, liste des joueurs, bio et chat */

#include "server.h"

int trouverParPseudo(Serveur *s, const char *pseudo); /* indice dans s->clients, ou -1 */
int trouverParSocket(Serveur *s, SOCKET sock);        /* indice dans s->clients, ou -1 */

/* envoie "INFO <texte>" à tous les joueurs identifiés, sauf celui d'indice sauf (-1 : personne) */
void annoncer(Serveur *s, int sauf, const char *format, ...);

void cmdLogin(Serveur *s, int i, char *args);
void cmdList(Serveur *s, int i, char *args);
void cmdBio(Serveur *s, int i, char *args);
void cmdBioAdd(Serveur *s, int i, char *args);
void cmdBioClear(Serveur *s, int i, char *args);
void cmdMsg(Serveur *s, int i, char *args);
void cmdAll(Serveur *s, int i, char *args);

#endif
