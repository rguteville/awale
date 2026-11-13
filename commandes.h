#ifndef COMMANDES_H
#define COMMANDES_H

#include "server.h"

/* traite une ligne complète reçue du client i ("PLAY 3", "MSG bob salut"...) */
void traiterLigne(Serveur *s, int i, char *ligne);

#endif
