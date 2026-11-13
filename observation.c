#include <stdio.h>
#include <stdlib.h>

#include "observation.h"
#include "joueurs.h" /* trouverParSocket */
#include "parties.h" /* formaterPlateau */
#include "social.h"  /* estAmi */
#include "envoi.h"

/* Les fonctions cmdXxx ne vérifient pas l'état du client :
   la table de commandes.c ne les appelle que dans les états autorisés. */

static int trouverPartieParId(Serveur *s, int idPartie)
{
   for (int k = 0; k < MAX_PARTIES; k++)
   {
      if (s->parties[k].active && s->parties[k].id == idPartie)
      {
         return k;
      }
   }
   return -1;
}

void retirerObservateur(Serveur *s, int i)
{
   Partie *partie = &s->parties[s->clients[i].partie];
   for (int k = 0; k < partie->nbObservateurs; k++)
   {
      if (partie->observateurs[k] == s->clients[i].sock)
      {
         partie->observateurs[k] = partie->observateurs[partie->nbObservateurs - 1];
         partie->nbObservateurs--;
         break;
      }
   }
   s->clients[i].etat = LIBRE;
   s->clients[i].partie = -1;
}

void cmdGames(Serveur *s, int i, char *args)
{
   (void)args;
   char ligne[BUF_SIZE];
   int pos = snprintf(ligne, sizeof ligne, "GAMES");
   for (int k = 0; k < MAX_PARTIES; k++)
   {
      if (s->parties[k].active)
      {
         /* pseudos pris dans l'historique : une partie en pause (joueur absent) est aussi listée */
         ajouterA(ligne, sizeof ligne, &pos, " %d:%s:%s", s->parties[k].id,
                         s->parties[k].historique.joueurs[0], s->parties[k].historique.joueurs[1]);
      }
   }
   envoyer(s->clients[i].sock, "%s", ligne);
}

void cmdWatch(Serveur *s, int i, char *args)
{
   int idPartie = atoi(args);
   if (idPartie <= 0)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX usage : WATCH <id>");
      return;
   }
   int k = trouverPartieParId(s, idPartie);
   if (k == -1)
   {
      envoyer(s->clients[i].sock, "ERR NOTFOUND partie %d introuvable", idPartie);
      return;
   }
   Partie *partie = &s->parties[k];

   /* partie privée : seuls les amis d'au moins un des deux joueurs peuvent observer.
      privee est fixé à l'ACCEPT : un joueur privé qui se déconnecte (pause) reste protégé */
   int partiePrivee = partie->historique.privee;
   int autorise = 0;
   for (int camp = 0; camp < 2; camp++)
   {
      int j = trouverParSocket(s, partie->joueurs[camp]);
      if (j == -1)
      {
         continue; /* joueur absent (partie en pause) : sa liste d'amis est perdue de toute façon */
      }
      if (s->clients[j].prive)
      {
         partiePrivee = 1; /* passé en PRIVATE ON pendant la partie */
      }
      if (estAmi(s, j, s->clients[i].name))
      {
         autorise = 1;
      }
   }
   if (partiePrivee && !autorise)
   {
      envoyer(s->clients[i].sock, "ERR DENIED partie %d privee : seuls les amis des joueurs peuvent l'observer", idPartie);
      return;
   }

   if (partie->nbObservateurs >= MAX_OBSERVATEURS)
   {
      envoyer(s->clients[i].sock, "ERR FULL trop d'observateurs pour la partie %d", idPartie);
      return;
   }
   partie->observateurs[partie->nbObservateurs] = s->clients[i].sock;
   partie->nbObservateurs++;
   s->clients[i].etat = OBSERVATEUR;
   s->clients[i].partie = k;
   envoyer(s->clients[i].sock, "OK WATCH %d", idPartie);

   /* le plateau actuel, au nouvel observateur SEULEMENT */
   char ligne[BUF_SIZE];
   formaterPlateau(partie->id, &partie->jeu, ligne, sizeof ligne);
   envoyer(s->clients[i].sock, "%s", ligne);
}

void cmdUnwatch(Serveur *s, int i, char *args)
{
   (void)args;
   retirerObservateur(s, i);
   envoyer(s->clients[i].sock, "OK UNWATCH");
}
