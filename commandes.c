#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "commandes.h"
#include "envoi.h"
#include "joueurs.h"
#include "parties.h"
#include "observation.h"
#include "social.h"
#include "archives.h"

/* ---------------------------------------------------------------------------
   La table des commandes.

   Chaque ligne dit : le nom de la commande, la fonction qui la traite, les états
   dans lesquels elle est autorisée, son usage et sa description (pour HELP),
   et le message d'erreur si on la tape dans un mauvais état.

   Ajouter une commande = écrire sa fonction cmdXxx(Serveur *s, int i, char *args)
   puis ajouter UNE ligne ici. traiterLigne et HELP n'ont pas à changer.
   --------------------------------------------------------------------------- */

typedef void (*FonctionCommande)(Serveur *s, int i, char *args);

typedef struct
{
   const char *nom;
   FonctionCommande fonction;
   int etats; /* ensemble d'états autorisés, voir ETAT() */
   const char *usage;
   const char *description;
   const char *siMauvaisEtat; /* texte de ERR STATE */
} Commande;

/* un ensemble d'états tient dans un int : le bit numéro e vaut 1 si l'état e est autorisé.
   ETAT(LIBRE) | ETAT(OBSERVATEUR) = "LIBRE ou OBSERVATEUR" */
#define ETAT(e) (1 << (e))
#define IDENTIFIE (ETAT(LIBRE) | ETAT(DEFI_ENVOYE) | ETAT(DEFI_RECU) | ETAT(EN_PARTIE) | ETAT(OBSERVATEUR))
#define TOUS (IDENTIFIE | ETAT(NON_IDENTIFIE))

static void cmdHelp(Serveur *s, int i, char *args);
static void cmdQuit(Serveur *s, int i, char *args);

static const Commande COMMANDES[] = {
    /* connexion */
    {"LOGIN", cmdLogin, ETAT(NON_IDENTIFIE), "LOGIN <pseudo>", "se connecter", "vous etes deja connecte"},
    {"HELP", cmdHelp, TOUS, "HELP", "commandes disponibles maintenant", NULL},
    {"QUIT", cmdQuit, TOUS, "QUIT", "quitter (abandonne la partie en cours)", NULL},
    {"LIST", cmdList, IDENTIFIE, "LIST", "joueurs connectes", NULL},
    /* défis et parties */
    {"CHALLENGE", cmdChallenge, ETAT(LIBRE), "CHALLENGE <pseudo>", "defier un joueur", "vous avez deja un defi ou une partie en cours"},
    {"ACCEPT", cmdAccept, ETAT(DEFI_RECU), "ACCEPT", "accepter le defi recu", "personne ne vous a defie"},
    {"REFUSE", cmdRefuse, ETAT(DEFI_RECU), "REFUSE", "refuser le defi recu", "personne ne vous a defie"},
    {"CANCEL", cmdCancel, ETAT(DEFI_ENVOYE), "CANCEL", "annuler votre defi", "vous n'avez pas lance de defi"},
    {"PLAY", cmdPlay, ETAT(EN_PARTIE), "PLAY <1-6>", "jouer la n-ieme case de votre rangee (de gauche a droite)", "vous n'etes pas en partie"},
    {"FORFEIT", cmdForfeit, ETAT(EN_PARTIE), "FORFEIT", "abandonner la partie", "vous n'etes pas en partie"},
    /* observation */
    {"GAMES", cmdGames, IDENTIFIE, "GAMES", "parties en cours", NULL},
    {"WATCH", cmdWatch, ETAT(LIBRE), "WATCH <id>", "observer une partie en cours", "terminez d'abord ce que vous faites (partie, defi ou observation)"},
    {"UNWATCH", cmdUnwatch, ETAT(OBSERVATEUR), "UNWATCH", "arreter d'observer", "vous n'observez aucune partie"},
    /* chat */
    {"MSG", cmdMsg, IDENTIFIE, "MSG <pseudo> <texte>", "message prive", NULL},
    {"ALL", cmdAll, IDENTIFIE, "ALL <texte>", "message a tous les joueurs", NULL},
    /* bio */
    {"BIO", cmdBio, IDENTIFIE, "BIO [pseudo]", "afficher une bio (la votre par defaut)", NULL},
    {"BIOADD", cmdBioAdd, IDENTIFIE, "BIOADD <ligne>", "ajouter une ligne a votre bio", NULL},
    {"BIOCLEAR", cmdBioClear, IDENTIFIE, "BIOCLEAR", "effacer votre bio", NULL},
    /* amis et mode privé */
    {"FRIEND", cmdFriend, IDENTIFIE, "FRIEND <pseudo>", "ajouter un ami (il pourra voir vos parties privees)", NULL},
    {"UNFRIEND", cmdUnfriend, IDENTIFIE, "UNFRIEND <pseudo>", "retirer un ami", NULL},
    {"FRIENDS", cmdFriends, IDENTIFIE, "FRIENDS", "votre liste d'amis", NULL},
    {"PRIVATE", cmdPrivate, IDENTIFIE, "PRIVATE ON|OFF", "parties visibles par vos amis seulement, ou par tous", NULL},
    /* parties terminées */
    {"HISTORY", cmdHistory, IDENTIFIE, "HISTORY [pseudo]", "10 dernieres parties terminees", NULL},
    {"RANK", cmdRank, IDENTIFIE, "RANK [pseudo]", "classement Elo (les 10 meilleurs, ou un joueur)", NULL},
    {"REPLAY", cmdReplay, ETAT(LIBRE), "REPLAY <id>", "revoir une partie terminee", "terminez d'abord ce que vous faites (partie, defi ou observation)"},
};

#define NB_COMMANDES ((int)(sizeof COMMANDES / sizeof COMMANDES[0]))

static int autorisee(const Commande *c, enum Etat etat)
{
   return (c->etats & ETAT(etat)) != 0;
}

/* HELP : une ligne HELPLINE par commande autorisée dans l'état actuel, puis HELPEND */
static void cmdHelp(Serveur *s, int i, char *args)
{
   (void)args;
   for (int k = 0; k < NB_COMMANDES; k++)
   {
      if (autorisee(&COMMANDES[k], s->clients[i].etat))
      {
         envoyer(s->clients[i].sock, "HELPLINE %s : %s", COMMANDES[k].usage, COMMANDES[k].description);
      }
   }
   envoyer(s->clients[i].sock, "HELPEND");
}

/* QUIT : un départ volontaire n'est pas une coupure réseau, donc pas de pause : on abandonne.
   La connexion est fermée par la boucle principale (server.c), qui regarde le champ quitte */
static void cmdQuit(Serveur *s, int i, char *args)
{
   (void)args;
   if (s->clients[i].etat == EN_PARTIE)
   {
      abandonnerPartie(s, i, "ABANDON");
   }
   envoyer(s->clients[i].sock, "OK QUIT");
   s->clients[i].quitte = 1;
}

void traiterLigne(Serveur *s, int i, char *ligne)
{
   /* ligne vide (l'utilisateur a juste tapé Entrée) : on l'ignore */
   if (ligne[0] == '\0')
   {
      return;
   }
   /* découpe "COMMANDE reste de la ligne" en deux chaînes */
   char *nom = ligne;
   char *args = strchr(ligne, ' ');
   if (args != NULL)
   {
      *args = '\0';
      args++;
   }
   else
   {
      args = "";
   }
   /* la commande est acceptée en minuscules aussi : "play 3" marche comme "PLAY 3" */
   for (int k = 0; nom[k] != '\0'; k++)
   {
      nom[k] = toupper((unsigned char)nom[k]);
   }

   for (int k = 0; k < NB_COMMANDES; k++)
   {
      const Commande *c = &COMMANDES[k];
      if (strcmp(nom, c->nom) != 0)
      {
         continue;
      }
      if (!autorisee(c, s->clients[i].etat))
      {
         if (s->clients[i].etat == NON_IDENTIFIE)
         {
            envoyer(s->clients[i].sock, "ERR STATE identifiez-vous d'abord avec LOGIN <pseudo>");
         }
         else
         {
            envoyer(s->clients[i].sock, "ERR STATE %s",
                    c->siMauvaisEtat != NULL ? c->siMauvaisEtat : "commande impossible maintenant");
         }
         return;
      }
      c->fonction(s, i, args);
      return;
   }
   envoyer(s->clients[i].sock, "ERR UNKNOWN commande inconnue %s (tapez HELP)", nom);
}
