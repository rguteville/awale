#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parties.h"
#include "joueurs.h" /* trouverParPseudo, trouverParSocket */
#include "envoi.h"
#include "classement.h"

/* Les fonctions cmdXxx ne vérifient pas l'état du client :
   la table de commandes.c ne les appelle que dans les états autorisés. */

static int campDuClient(const Partie *partie, SOCKET sock)
{
   return (partie->joueurs[0] == sock) ? 0 : 1;
}

/* envoie la même ligne aux joueurs présents et aux observateurs.
   envoyer() ignore INVALID_SOCKET : un joueur déconnecté est simplement sauté */
static void prevenirPartie(const Partie *partie, const char *ligne)
{
   for (int k = 0; k < 2; k++)
   {
      envoyer(partie->joueurs[k], "%s", ligne);
   }
   for (int k = 0; k < partie->nbObservateurs; k++)
   {
      envoyer(partie->observateurs[k], "%s", ligne);
   }
}

void formaterPlateau(int id, const struct Jeu *jeu, char *ligne, size_t taille)
{
   int pos = snprintf(ligne, taille, "BOARD %d", id);
   for (int k = 0; k < NB_CASES; k++)
   {
      ajouterA(ligne, taille, &pos, " %d", jeu->plateau[k]);
   }
   ajouterA(ligne, taille, &pos, " %d %d %d", jeu->score[0], jeu->score[1], jeu->joueurCourant);
}

static void envoyerPlateau(const Partie *partie)
{
   char ligne[BUF_SIZE];
   formaterPlateau(partie->id, &partie->jeu, ligne, sizeof ligne);
   prevenirPartie(partie, ligne);
}

/* remet deux clients LIBRE, sans défi en cours */
static void libererDefi(Serveur *s, int i, int j)
{
   s->clients[i].etat = LIBRE;
   s->clients[i].adversaire = INVALID_SOCKET;
   if (j != -1)
   {
      s->clients[j].etat = LIBRE;
      s->clients[j].adversaire = INVALID_SOCKET;
   }
}

void cmdChallenge(Serveur *s, int i, char *args)
{
   if (args[0] == '\0')
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX usage : CHALLENGE <pseudo>");
      return;
   }
   int j = trouverParPseudo(s, args);
   if (j == -1)
   {
      envoyer(s->clients[i].sock, "ERR NOTFOUND pseudo inconnu");
      return;
   }
   if (j == i)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX vous ne pouvez pas vous defier vous-meme");
      return;
   }
   if (s->clients[j].etat != LIBRE)
   {
      envoyer(s->clients[i].sock, "ERR BUSY %s n'est pas libre", s->clients[j].name);
      return;
   }
   s->clients[i].etat = DEFI_ENVOYE;
   s->clients[i].adversaire = s->clients[j].sock;
   s->clients[j].etat = DEFI_RECU;
   s->clients[j].adversaire = s->clients[i].sock;
   envoyer(s->clients[i].sock, "OK CHALLENGE %s", s->clients[j].name);
   envoyer(s->clients[j].sock, "CHALLENGED %s", s->clients[i].name);
}

void cmdRefuse(Serveur *s, int i, char *args)
{
   (void)args;
   int j = trouverParSocket(s, s->clients[i].adversaire);
   libererDefi(s, i, j);
   envoyer(s->clients[i].sock, "OK REFUSE");
   if (j != -1)
   {
      envoyer(s->clients[j].sock, "REFUSED %s", s->clients[i].name);
   }
}

void cmdCancel(Serveur *s, int i, char *args)
{
   (void)args;
   int j = trouverParSocket(s, s->clients[i].adversaire);
   libererDefi(s, i, j);
   envoyer(s->clients[i].sock, "OK CANCEL");
   if (j != -1)
   {
      envoyer(s->clients[j].sock, "CANCELLED %s", s->clients[i].name);
   }
}

void annulerDefi(Serveur *s, int i)
{
   int j = trouverParSocket(s, s->clients[i].adversaire);
   libererDefi(s, i, j);
   if (j != -1)
   {
      envoyer(s->clients[j].sock, "CANCELLED %s", s->clients[i].name);
   }
}

void cmdAccept(Serveur *s, int i, char *args)
{
   (void)args;
   int j = trouverParSocket(s, s->clients[i].adversaire);
   if (j == -1)
   {
      envoyer(s->clients[i].sock, "ERR STATE le defi n'est plus valide");
      libererDefi(s, i, -1);
      return;
   }
   Partie *partie = NULL;
   for (int k = 0; k < MAX_PARTIES; k++)
   {
      if (s->parties[k].active == 0)
      {
         partie = &s->parties[k];
         break;
      }
   }
   if (partie == NULL)
   {
      envoyer(s->clients[i].sock, "ERR BUSY trop de parties en cours, reessayez plus tard");
      envoyer(s->clients[j].sock, "ERR BUSY trop de parties en cours, reessayez plus tard");
      libererDefi(s, i, j);
      return;
   }
   /* tirage au sort : le camp 0 commence toujours, on tire donc qui reçoit le camp 0 */
   int campDeI = rand() % 2;
   partie->active = 1;
   partie->id = s->prochainId++;
   partie->joueurs[campDeI] = s->clients[i].sock;
   partie->joueurs[1 - campDeI] = s->clients[j].sock;
   partie->deconnecteDepuis[0] = 0;
   partie->deconnecteDepuis[1] = 0;
   partie->nbObservateurs = 0;
   partie->historique.id = partie->id;
   strcpy(partie->historique.joueurs[campDeI], s->clients[i].name);
   strcpy(partie->historique.joueurs[1 - campDeI], s->clients[j].name);
   partie->historique.privee = s->clients[i].prive || s->clients[j].prive;
   partie->historique.nbCoups = 0;

   initJeu(&partie->jeu, 0);

   s->clients[i].partie = partie - s->parties; /* indice de la partie dans le tableau */
   s->clients[j].partie = partie - s->parties;
   s->clients[i].etat = EN_PARTIE;
   s->clients[j].etat = EN_PARTIE;

   envoyer(s->clients[i].sock, "START %d %s %d", partie->id, s->clients[j].name, campDeI);
   envoyer(s->clients[j].sock, "START %d %s %d", partie->id, s->clients[i].name, 1 - campDeI);
   envoyerPlateau(partie);
   journal("partie %d : %s contre %s%s", partie->id, partie->historique.joueurs[0],
           partie->historique.joueurs[1], partie->historique.privee ? " (privee)" : "");
}

void cmdPlay(Serveur *s, int i, char *args)
{
   int caseJouee = atoi(args);
   if (caseJouee < 1 || caseJouee > 6)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX case de 1 a 6 uniquement");
      return;
   }
   Partie *partie = &s->parties[s->clients[i].partie];
   if (partie->deconnecteDepuis[0] != 0 || partie->deconnecteDepuis[1] != 0)
   {
      envoyer(s->clients[i].sock, "ERR PAUSED partie en pause, attendez le retour de votre adversaire");
      return;
   }
   int monCamp = campDuClient(partie, s->clients[i].sock);
   if (partie->jeu.joueurCourant != monCamp)
   {
      envoyer(s->clients[i].sock, "ERR NOTYOURTURN ce n'est pas votre tour");
      return;
   }
   /* n compté de gauche à droite sur l'affichage : rangée du bas = cases 0..5, rangée du haut = 11..6 */
   int numCaseCliquee = (monCamp == 0) ? (caseJouee - 1) : (12 - caseJouee);
   if (jouerCoup(&partie->jeu, numCaseCliquee) < 0)
   {
      if (partie->jeu.plateau[numCaseCliquee] == 0)
      {
         envoyer(s->clients[i].sock, "ERR ILLEGAL la case %d est vide", caseJouee);
      }
      else
      {
         envoyer(s->clients[i].sock, "ERR ILLEGAL ce coup laisserait votre adversaire sans graine");
      }
      return;
   }
   partie->historique.coups[partie->historique.nbCoups] = numCaseCliquee;
   partie->historique.nbCoups++;
   envoyerPlateau(partie);
   if (partie->jeu.fini)
   {
      const char *raison;
      if (partie->jeu.score[0] > 24 || partie->jeu.score[1] > 24)
      {
         raison = "SCORE";
      }
      else if (partie->jeu.nbCoups >= NB_COUPS_MAX)
      {
         raison = "LIMITE";
      }
      else
      {
         raison = "BLOQUE";
      }
      /* les pseudos des deux camps sont dans l'historique : pas besoin de chercher les clients */
      int g = gagnant(&partie->jeu);
      const char *pseudoGagnant = (g == -1) ? "NUL" : partie->historique.joueurs[g];
      terminerPartie(s, partie, pseudoGagnant, raison);
   }
}

void cmdForfeit(Serveur *s, int i, char *args)
{
   (void)args;
   abandonnerPartie(s, i, "ABANDON");
}

void abandonnerPartie(Serveur *s, int i, const char *raison)
{
   /* le gagnant est l'autre camp, même s'il est déconnecté en ce moment (partie en pause) */
   Partie *partie = &s->parties[s->clients[i].partie];
   int monCamp = campDuClient(partie, s->clients[i].sock);
   terminerPartie(s, partie, partie->historique.joueurs[1 - monCamp], raison);
}

void terminerPartie(Serveur *s, Partie *partie, const char *gagnant, const char *raison)
{
   char ligne[BUF_SIZE];
   snprintf(ligne, sizeof ligne, "END %d %s %s", partie->id, gagnant, raison);
   prevenirPartie(partie, ligne);

   /* joueurs présents et observateurs redeviennent LIBRE */
   for (int k = 0; k < 2; k++)
   {
      int j = trouverParSocket(s, partie->joueurs[k]); /* -1 si le joueur est déconnecté */
      if (j != -1)
      {
         s->clients[j].etat = LIBRE;
         s->clients[j].adversaire = INVALID_SOCKET;
         s->clients[j].partie = -1;
      }
   }
   for (int k = 0; k < partie->nbObservateurs; k++)
   {
      int j = trouverParSocket(s, partie->observateurs[k]);
      if (j != -1)
      {
         s->clients[j].etat = LIBRE;
         s->clients[j].partie = -1;
      }
   }
   partie->nbObservateurs = 0;
   strcpy(partie->historique.gagnant, gagnant);
   strcpy(partie->historique.raison, raison);
   sauverPartie(&partie->historique);

   /* classement Elo : chaque joueur présent reçoit "ELO <ancien> <nouveau>" juste après END */
   int g = -1; /* -1 : match nul */
   if (strcmp(gagnant, "NUL") != 0)
   {
      g = (strcmp(gagnant, partie->historique.joueurs[0]) == 0) ? 0 : 1;
   }
   int ancien[2], nouveau[2];
   enregistrerResultat(partie->historique.joueurs[0], partie->historique.joueurs[1], g, ancien, nouveau);
   for (int k = 0; k < 2; k++)
   {
      envoyer(partie->joueurs[k], "ELO %d %d", ancien[k], nouveau[k]);
   }

   partie->deconnecteDepuis[0] = 0;
   partie->deconnecteDepuis[1] = 0;
   partie->active = 0;
   journal("partie %d terminee : %s (%s)", partie->id,
           strcmp(gagnant, "NUL") == 0 ? "match nul" : gagnant, raison);
}

void suspendrePartie(Serveur *s, int i)
{
   Partie *partie = &s->parties[s->clients[i].partie];
   int camp = campDuClient(partie, s->clients[i].sock);

   /* la partie ne connaît plus la socket de ce joueur : elle le retrouvera par son PSEUDO
      (partie->historique.joueurs[camp]) quand il refera LOGIN */
   partie->joueurs[camp] = INVALID_SOCKET;
   partie->deconnecteDepuis[camp] = time(NULL);

   s->clients[i].etat = LIBRE;
   s->clients[i].partie = -1;
   s->clients[i].adversaire = INVALID_SOCKET;

   char ligne[BUF_SIZE];
   snprintf(ligne, sizeof ligne, "PAUSE %d %s %d", partie->id, partie->historique.joueurs[camp], DELAI_RECONNEXION);
   prevenirPartie(partie, ligne);
   journal("partie %d en pause : %s a %d s pour revenir", partie->id, partie->historique.joueurs[camp], DELAI_RECONNEXION);
}

void reprendrePartie(Serveur *s, int i)
{
   Client *client = &s->clients[i];
   for (int k = 0; k < MAX_PARTIES; k++)
   {
      Partie *partie = &s->parties[k];
      if (!partie->active)
      {
         continue;
      }
      for (int camp = 0; camp < 2; camp++)
      {
         if (partie->joueurs[camp] != INVALID_SOCKET || strcmp(partie->historique.joueurs[camp], client->name) != 0)
         {
            continue;
         }
         char ligne[BUF_SIZE];

         /* prévenir les autres AVANT de rattacher la socket : celui qui revient reçoit RESUME à la place */
         snprintf(ligne, sizeof ligne, "RESUMED %d %s", partie->id, client->name);
         prevenirPartie(partie, ligne);

         partie->joueurs[camp] = client->sock;
         partie->deconnecteDepuis[camp] = 0;
         client->etat = EN_PARTIE;
         client->partie = k;
         client->adversaire = partie->joueurs[1 - camp]; /* INVALID_SOCKET si l'autre est absent aussi */

         /* l'adversaire (s'il est là) gardait l'ANCIENNE socket : on la remplace */
         int j = trouverParSocket(s, partie->joueurs[1 - camp]);
         if (j != -1)
         {
            s->clients[j].adversaire = client->sock;
         }

         envoyer(client->sock, "RESUME %d %s %d", partie->id, partie->historique.joueurs[1 - camp], camp);
         formaterPlateau(partie->id, &partie->jeu, ligne, sizeof ligne);
         envoyer(client->sock, "%s", ligne);

         /* l'adversaire est peut-être lui aussi déconnecté : la pause continue */
         if (partie->deconnecteDepuis[1 - camp] != 0)
         {
            int reste = DELAI_RECONNEXION - (int)difftime(time(NULL), partie->deconnecteDepuis[1 - camp]);
            envoyer(client->sock, "PAUSE %d %s %d", partie->id, partie->historique.joueurs[1 - camp], reste > 0 ? reste : 0);
         }
         journal("partie %d reprise : %s de retour", partie->id, client->name);
         return; /* un pseudo est dans au plus une partie */
      }
   }
}

void verifierDelais(Serveur *s)
{
   time_t maintenant = time(NULL);
   for (int k = 0; k < MAX_PARTIES; k++)
   {
      Partie *partie = &s->parties[k];
      if (!partie->active)
      {
         continue;
      }
      for (int camp = 0; camp < 2; camp++)
      {
         if (partie->deconnecteDepuis[camp] != 0 &&
             difftime(maintenant, partie->deconnecteDepuis[camp]) >= DELAI_RECONNEXION)
         {
            /* l'autre joueur est absent lui aussi : match nul. Sinon il gagne. */
            const char *pseudoGagnant = (partie->deconnecteDepuis[1 - camp] != 0)
                                            ? "NUL"
                                            : partie->historique.joueurs[1 - camp];
            terminerPartie(s, partie, pseudoGagnant, "DECONNEXION");
            break; /* la partie est terminée : on ne regarde pas l'autre camp */
         }
      }
   }
}
