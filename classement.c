#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h> /* pow : penser à -lm sous Linux (voir makefile) */

#include "classement.h"

#define FICHIER_CLASSEMENT "classement.txt"

/* static : un tableau de 1000 fiches (~36 Ko) n'a rien à faire sur la pile */
static FicheJoueur fiches[CLASSEMENT_MAX];

/* lit tout le fichier dans fiches ; renvoie le nombre de fiches. Pas de fichier = classement vide */
static int charger(void)
{
   FILE *f = fopen(FICHIER_CLASSEMENT, "r");
   if (f == NULL)
   {
      return 0;
   }
   int n = 0;
   char ligne[256];
   while (n < CLASSEMENT_MAX && fgets(ligne, sizeof ligne, f) != NULL)
   {
      FicheJoueur *p = &fiches[n];
      /* on ne fait pas confiance au fichier : une ligne mal formée est ignorée */
      if (sscanf(ligne, "%20s %d %d %d %d", p->pseudo, &p->elo, &p->victoires, &p->nuls, &p->defaites) == 5)
      {
         n++;
      }
   }
   fclose(f);
   return n;
}

static void sauver(int n)
{
   FILE *f = fopen(FICHIER_CLASSEMENT, "w"); /* "w" : on réécrit tout, comme pour la bio */
   if (f == NULL)
   {
      perror(FICHIER_CLASSEMENT);
      return;
   }
   for (int k = 0; k < n; k++)
   {
      fprintf(f, "%s %d %d %d %d\n", fiches[k].pseudo, fiches[k].elo, fiches[k].victoires, fiches[k].nuls, fiches[k].defaites);
   }
   fclose(f);
}

/* indice de la fiche de pseudo ; la crée (Elo de départ) si le joueur n'a jamais été classé.
   -1 si le classement est plein */
static int trouverOuCreer(const char *pseudo, int *n)
{
   for (int k = 0; k < *n; k++)
   {
      if (strcmp(fiches[k].pseudo, pseudo) == 0)
      {
         return k;
      }
   }
   if (*n == CLASSEMENT_MAX)
   {
      return -1;
   }
   FicheJoueur *p = &fiches[*n];
   snprintf(p->pseudo, sizeof p->pseudo, "%s", pseudo);
   p->elo = ELO_DEPART;
   p->victoires = 0;
   p->nuls = 0;
   p->defaites = 0;
   (*n)++;
   return *n - 1;
}

/* Formule d'Elo :
   - score attendu de A contre B : E = 1 / (1 + 10^((eloB - eloA) / 400))
     (deux joueurs de même niveau : E = 0,5 ; A beaucoup plus fort : E proche de 1)
   - après la partie : eloA += K * (S - E), avec S = 1 si A gagne, 0,5 si nul, 0 s'il perd.
   Battre plus fort que soi rapporte donc beaucoup, battre plus faible rapporte peu. */
static int nouvelElo(int eloA, int eloB, double resultatA)
{
   double attendu = 1.0 / (1.0 + pow(10.0, (eloB - eloA) / 400.0));
   double variation = ELO_K * (resultatA - attendu);
   /* arrondi à l'entier le plus proche, positif ou négatif */
   return eloA + (int)(variation >= 0 ? variation + 0.5 : variation - 0.5);
}

void enregistrerResultat(const char *joueur0, const char *joueur1, int gagnant, int ancien[2], int nouveau[2])
{
   int n = charger();
   int k0 = trouverOuCreer(joueur0, &n);
   int k1 = trouverOuCreer(joueur1, &n);
   if (k0 == -1 || k1 == -1)
   {
      fprintf(stderr, "classement plein (%d joueurs) : partie non classee\n", CLASSEMENT_MAX);
      ancien[0] = nouveau[0] = ELO_DEPART;
      ancien[1] = nouveau[1] = ELO_DEPART;
      return;
   }
   FicheJoueur *f[2] = {&fiches[k0], &fiches[k1]};

   for (int k = 0; k < 2; k++)
   {
      ancien[k] = f[k]->elo;
   }
   for (int k = 0; k < 2; k++)
   {
      double resultat = (gagnant == -1) ? 0.5 : (gagnant == k ? 1.0 : 0.0);
      /* calculé avec les ANCIENS Elo des deux joueurs */
      nouveau[k] = nouvelElo(ancien[k], ancien[1 - k], resultat);
      f[k]->elo = nouveau[k];
      if (gagnant == -1)
      {
         f[k]->nuls++;
      }
      else if (gagnant == k)
      {
         f[k]->victoires++;
      }
      else
      {
         f[k]->defaites++;
      }
   }
   sauver(n);
}

/* pour qsort : renvoie < 0 si a doit passer avant b. Meilleur Elo d'abord, puis le plus de victoires */
static int comparerFiches(const void *a, const void *b)
{
   const FicheJoueur *fa = a;
   const FicheJoueur *fb = b;
   if (fa->elo != fb->elo)
   {
      return fb->elo - fa->elo;
   }
   return fb->victoires - fa->victoires;
}

int lireClassement(FicheJoueur *res, int max)
{
   int n = charger();
   qsort(fiches, n, sizeof fiches[0], comparerFiches);
   if (n > max)
   {
      n = max;
   }
   memcpy(res, fiches, n * sizeof fiches[0]);
   return n;
}
