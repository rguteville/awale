#ifndef ARCHIVES_H
#define ARCHIVES_H

/* Parties terminées : HISTORY et REPLAY (parties.txt, via sauvegarde.c) et classement Elo (classement.c) */

#include "server.h"

void cmdHistory(Serveur *s, int i, char *args);
void cmdReplay(Serveur *s, int i, char *args);
void cmdRank(Serveur *s, int i, char *args);   /* classement Elo */

#define CLASSEMENT_AFFICHE 10 /* nombre de joueurs affichés par RANK */

#endif
