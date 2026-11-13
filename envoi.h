#ifndef ENVOI_H
#define ENVOI_H

#include "plateforme.h"

/* envoie une ligne au client, comme printf ; le '\n' final est ajouté ici.
   Ne fait rien si sock vaut INVALID_SOCKET (joueur déconnecté, partie en pause). */
void envoyer(SOCKET sock, const char *format, ...);

/* ajoute un morceau formaté à la fin de ligne, en position *pos, SEULEMENT s'il tient en entier.
   Renvoie 1 si ajouté, 0 sinon (la ligne est pleine, rien n'est coupé en plein milieu d'un mot).
   Remplace "pos += snprintf(ligne + pos, taille - pos, ...)", qui déborde quand pos dépasse taille :
   taille - pos est un size_t, il devient alors un nombre énorme au lieu d'être négatif. */
int ajouterA(char *ligne, size_t taille, int *pos, const char *format, ...);

/* écrit une ligne horodatée dans le terminal du serveur, comme printf : "[14:02:31] alice connectée" */
void journal(const char *format, ...);

#endif
