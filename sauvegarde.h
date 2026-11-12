#ifndef SAUVEGARDE_H
#define SAUVEGARDE_H


#include "commun.h"
#include "awale.h"     

typedef struct
{
   int id;
   char joueurs[2][PSEUDO_MAX + 1];   
   char gagnant[PSEUDO_MAX + 1];     
   char raison[16];                   
   int privee;                       
   int coups[NB_COUPS_MAX];
   int nbCoups;           
} PartieSauvee;

void sauverPartie(const PartieSauvee *p);
int lireDernierId(void);
int chercherPartie(int id, PartieSauvee *p);
int listerParties(const char *pseudo, PartieSauvee *res, int max);
void sauverBio(const char *pseudo, char bio[][BIO_LIGNE_MAX + 1], int nbLignes);
int chargerBio(const char *pseudo, char bio[][BIO_LIGNE_MAX + 1], int max);

#endif
