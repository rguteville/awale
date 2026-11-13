#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archives.h"
#include "joueurs.h" /* trouverParPseudo */
#include "parties.h" /* formaterPlateau */
#include "social.h"  /* estAmi */
#include "envoi.h"
#include "classement.h"

void cmdHistory(Serveur *s, int i, char *args)
{
   /* pas de copie de args : un pseudo trop long ne correspondra simplement à aucune partie */
   const char *pseudo = (args[0] == '\0') ? s->clients[i].name : args;
   PartieSauvee res[HISTORIQUE_MAX];
   int n = listerParties(pseudo, res, HISTORIQUE_MAX);
   char ligne[BUF_SIZE];
   int pos = snprintf(ligne, sizeof ligne, "HISTORY");
   for (int k = 0; k < n; k++)
   {
      ajouterA(ligne, sizeof ligne, &pos, " %d:%s:%s:%s", res[k].id, res[k].joueurs[0], res[k].joueurs[1], res[k].gagnant);
   }
   envoyer(s->clients[i].sock, "%s", ligne);
}

/* une partie privée ne peut être revue que par ses joueurs et par les amis CONNECTÉS de l'un d'eux */
static int peutRevoir(Serveur *s, int i, const PartieSauvee *p)
{
   if (!p->privee)
   {
      return 1;
   }
   if (strcmp(s->clients[i].name, p->joueurs[0]) == 0 || strcmp(s->clients[i].name, p->joueurs[1]) == 0)
   {
      return 1;
   }
   for (int camp = 0; camp < 2; camp++)
   {
      /* la partie est finie : on cherche les joueurs par PSEUDO (ils n'ont plus de socket de partie) */
      int j = trouverParPseudo(s, p->joueurs[camp]);
      if (j != -1 && estAmi(s, j, s->clients[i].name))
      {
         return 1;
      }
   }
   return 0;
}

void cmdReplay(Serveur *s, int i, char *args)
{
   int id = atoi(args);
   if (id <= 0)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX usage : REPLAY <id>");
      return;
   }
   PartieSauvee partieSauvee;
   if (chercherPartie(id, &partieSauvee) == 0)
   {
      envoyer(s->clients[i].sock, "ERR NOTFOUND partie %d inconnue", id);
      return;
   }
   if (!peutRevoir(s, i, &partieSauvee))
   {
      envoyer(s->clients[i].sock, "ERR DENIED partie %d privee : seuls les joueurs et leurs amis peuvent la revoir", id);
      return;
   }
   envoyer(s->clients[i].sock, "OK REPLAY %d %s %s", id, partieSauvee.joueurs[0], partieSauvee.joueurs[1]);

   /* on rejoue les coups enregistrés avec le moteur : un BOARD par position, tout d'un coup.
      Pas de pause entre les BOARD : le serveur n'a qu'un fil d'exécution, un sleep gèlerait tout le monde */
   struct Jeu jeu;
   initJeu(&jeu, 0);
   char ligne[BUF_SIZE];
   formaterPlateau(id, &jeu, ligne, sizeof ligne);
   envoyer(s->clients[i].sock, "%s", ligne);
   for (int k = 0; k < partieSauvee.nbCoups; k++)
   {
      if (jouerCoup(&jeu, partieSauvee.coups[k]) < 0)
      {
         envoyer(s->clients[i].sock, "INFO partie %d : fichier de sauvegarde abime, rejeu interrompu", id);
         break;
      }
      formaterPlateau(id, &jeu, ligne, sizeof ligne);
      envoyer(s->clients[i].sock, "%s", ligne);
   }
   envoyer(s->clients[i].sock, "END %d %s %s", id, partieSauvee.gagnant, partieSauvee.raison);
}

/* RANK : les CLASSEMENT_AFFICHE meilleurs joueurs. RANK <pseudo> : la ligne de ce joueur seulement.
   Chaque joueur : <rang>:<pseudo>:<elo>:<victoires>:<nuls>:<defaites> */
void cmdRank(Serveur *s, int i, char *args)
{
   static FicheJoueur classement[CLASSEMENT_MAX]; /* static : trop gros pour la pile */
   int n = lireClassement(classement, CLASSEMENT_MAX);
   char ligne[BUF_SIZE];
   int pos = snprintf(ligne, sizeof ligne, "RANKING");
   for (int k = 0; k < n; k++)
   {
      int afficher = (args[0] == '\0') ? (k < CLASSEMENT_AFFICHE) : (strcmp(classement[k].pseudo, args) == 0);
      if (afficher)
      {
         ajouterA(ligne, sizeof ligne, &pos, " %d:%s:%d:%d:%d:%d", k + 1, classement[k].pseudo, classement[k].elo,
                  classement[k].victoires, classement[k].nuls, classement[k].defaites);
      }
   }
   envoyer(s->clients[i].sock, "%s", ligne);
}
