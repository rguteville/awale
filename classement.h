#ifndef CLASSEMENT_H
#define CLASSEMENT_H

/* Classement Elo, sauvegardé dans classement.txt (une ligne par joueur ayant fini une partie) :
   <pseudo> <elo> <victoires> <nuls> <defaites> */

#include "commun.h"

#define ELO_DEPART 1200
#define ELO_K      32   /* plus K est grand, plus une partie fait bouger l'Elo */
#define CLASSEMENT_MAX 1000

typedef struct
{
   char pseudo[PSEUDO_MAX + 1];
   int elo;
   int victoires;
   int nuls;
   int defaites;
} FicheJoueur;

/* enregistre le résultat d'une partie et met à jour l'Elo des deux joueurs.
   gagnant : 0 si joueurs[0] gagne, 1 si joueurs[1] gagne, -1 pour un match nul.
   ancien[k] et nouveau[k] reçoivent l'Elo du joueur k avant et après la partie */
void enregistrerResultat(const char *joueur0, const char *joueur1, int gagnant, int ancien[2], int nouveau[2]);

/* remplit res avec les joueurs classés du meilleur au moins bon ; renvoie leur nombre (au plus max) */
int lireClassement(FicheJoueur *res, int max);

#endif
