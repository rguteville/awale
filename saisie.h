#ifndef SAISIE_H
#define SAISIE_H

#include "client.h"

/* traite une ligne tapée par le joueur : raccourcis ("3", "o", "@bob salut"), commandes en français
   ("defier bob") ou commandes du protocole ("CHALLENGE bob"), puis envoie au serveur */
void traiterSaisie(EtatClient *e, char *ligne);

#endif
