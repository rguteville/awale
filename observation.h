#ifndef OBSERVATION_H
#define OBSERVATION_H

/* Liste des parties en cours et observation (WATCH / UNWATCH) */

#include "server.h"

void cmdGames(Serveur *s, int i, char *args);
void cmdWatch(Serveur *s, int i, char *args);
void cmdUnwatch(Serveur *s, int i, char *args);

void retirerObservateur(Serveur *s, int i); /* i n'observe plus sa partie (UNWATCH ou déconnexion) */

#endif
