#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sauvegarde.h"

/* Noms des fichiers : définis une seule fois */
#define FICHIER_PARTIES "parties.txt"

/* Les fonctions sont à écrire ici, dans cet ordre (voir le plan) :

   Étape 2 : void sauverPartie(const PartieSauvee *p)
             int  lireDernierId(void)

   Étape 3 : int  chercherPartie(int id, PartieSauvee *p)

   Étape 4 : int  listerParties(const char *pseudo, PartieSauvee *res, int max)

   Étape 5 : void sauverBio(const char *pseudo, char bio[][BIO_LIGNE_MAX + 1], int nbLignes)
             int  chargerBio(const char *pseudo, char bio[][BIO_LIGNE_MAX + 1], int max)

   Rappel : en cas d'échec de fopen -> perror, puis on rend la main (jamais exit). */

void sauverPartie(const PartieSauvee *p)
{
   FILE *f = fopen(FICHIER_PARTIES, "a");
   if (f == NULL)
   {
      perror("parties.txt");
      return;
   }
   fprintf(f, "%d %s %s %s %s %d %d", p->id, p->joueurs[0], p->joueurs[1], p->gagnant, p->raison, p->privee, p->nbCoups);
   for (int k = 0; k < p->nbCoups; k++)
   {
      fprintf(f, " %d", p->coups[k]);
   }
   fprintf(f, "\n");
   fclose(f);
}

int lireDernierId()
{
   FILE *f = fopen(FICHIER_PARTIES, "r");
   if (f == NULL)
   {
      return 0;
   }
   int max = 0;
   char ligne[1024];
   while (fgets(ligne, sizeof ligne, f) != NULL)
   {
      int id;
      if (sscanf(ligne, "%d", &id) == 1 && id > max)
      {
         max = id;
      }
   }
   fclose(f);
   return max;
}

int  chercherPartie(int id, PartieSauvee *p)
{
   FILE *f = fopen(FICHIER_PARTIES, "r");
   if (f == NULL)
   {
      return 0;
   }
   char ligne[1024];
   while (fgets(ligne, sizeof ligne, f) != NULL)
   {
      int lu;
      if (sscanf(ligne, "%d", &lu) != 1 || lu != id )
      {
         continue;
      }
      int pos;
      if (sscanf(ligne, "%d %20s %20s %20s %15s %d %d%n", &p->id, p->joueurs[0], p->joueurs[1], p->gagnant, p->raison, &p->privee, &p->nbCoups, &pos) != 7 || p->nbCoups < 0 || p->nbCoups > NB_COUPS_MAX)
      {
         fclose(f);
         return 0;
      }
      char * mot = strtok(ligne + pos, " \n");
      for (int k = 0; k < p->nbCoups; k++)
      {
         if (mot == NULL)
         {
            fclose(f);
            return 0;
         }
         p->coups[k] = atoi(mot);
         mot = strtok(NULL, " \n");
      }
      fclose(f);
      return 1;
   }
   fclose(f);
   return 0;
}