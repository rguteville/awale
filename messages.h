#ifndef MESSAGES_H
#define MESSAGES_H

#include "client.h"

/* traite une ligne reçue du serveur ("BOARD ...", "CHALLENGED bob"...) : met à jour e et affiche */
void traiterMessage(EtatClient *e, char *ligne);

/* affiche la position courante du rejeu (REPLAY) ; utilisée aussi par saisie.c pour naviguer */
void afficherPositionRejeu(EtatClient *e);

#endif
