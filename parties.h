#ifndef PARTIES_H
#define PARTIES_H

/* Défis, déroulement d'une partie, fin de partie, pause et reprise après déconnexion */

#include "server.h"

void cmdChallenge(Serveur *s, int i, char *args);
void cmdAccept(Serveur *s, int i, char *args);
void cmdRefuse(Serveur *s, int i, char *args);
void cmdCancel(Serveur *s, int i, char *args);
void cmdPlay(Serveur *s, int i, char *args);
void cmdForfeit(Serveur *s, int i, char *args);

/* écrit "BOARD <id> <c0> ... <c11> <score0> <score1> <joueur_courant>" dans ligne, n'envoie rien */
void formaterPlateau(int id, const struct Jeu *jeu, char *ligne, size_t taille);

void abandonnerPartie(Serveur *s, int i, const char *raison); /* i perd, son adversaire gagne */
void terminerPartie(Serveur *s, Partie *partie, const char *gagnant, const char *raison);

void annulerDefi(Serveur *s, int i);     /* i se déconnecte avec un défi en attente */
void suspendrePartie(Serveur *s, int i); /* i se déconnecte en partie : pause */
void reprendrePartie(Serveur *s, int i); /* après LOGIN : i avait une partie en pause ? */
void verifierDelais(Serveur *s);         /* termine les parties dont le délai de pause est dépassé */

#endif
