#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "saisie.h"
#include "affichage.h"
#include "messages.h" /* afficherPositionRejeu */

/* ---------------------------------------------------------------------------
   Les mots que le joueur peut taper, et la commande du protocole envoyée.
   Les commandes du protocole elles-mêmes (PLAY 3, CHALLENGE bob...) marchent aussi,
   en majuscules ou en minuscules : un mot inconnu de cette table est envoyé tel quel.
   --------------------------------------------------------------------------- */

typedef struct
{
   const char *mot;
   const char *commande;
} Alias;

static const Alias ALIAS[] = {
    {"joueurs", "LIST"},       {"liste", "LIST"},
    {"defier", "CHALLENGE"},   {"défier", "CHALLENGE"}, {"defi", "CHALLENGE"}, {"défi", "CHALLENGE"},
    {"accepter", "ACCEPT"},    {"refuser", "REFUSE"},   {"annuler", "CANCEL"},
    {"jouer", "PLAY"},         {"abandonner", "FORFEIT"}, {"abandon", "FORFEIT"},
    {"parties", "GAMES"},      {"observer", "WATCH"},   {"regarder", "WATCH"},
    {"arreter", "UNWATCH"},    {"arrêter", "UNWATCH"},  {"stop", "UNWATCH"},
    {"tous", "ALL"},
    {"bioajout", "BIOADD"},    {"bioeffacer", "BIOCLEAR"},
    {"ami", "FRIEND"},         {"plusami", "UNFRIEND"}, {"amis", "FRIENDS"},
    {"prive", "PRIVATE"},      {"privé", "PRIVATE"},
    {"historique", "HISTORY"}, {"revoir", "REPLAY"},
    {"classement", "RANK"},    {"elo", "RANK"},
    {"quitter", "QUIT"},       {"exit", "QUIT"},
};

#define NB_ALIAS ((int)(sizeof ALIAS / sizeof ALIAS[0]))

static int estDansListe(const char *mot, const char *liste[])
{
   for (int k = 0; liste[k] != NULL; k++)
   {
      if (strcmp(mot, liste[k]) == 0)
      {
         return 1;
      }
   }
   return 0;
}

static const char *OUI[] = {"o", "oui", "y", "yes", NULL};
static const char *NON[] = {"n", "non", "no", NULL};
static const char *AIDE[] = {"aide", "help", "?", NULL};

static int queDesChiffres(const char *s)
{
   if (s[0] == '\0')
   {
      return 0;
   }
   for (int k = 0; s[k] != '\0'; k++)
   {
      if (!isdigit((unsigned char)s[k]))
      {
         return 0;
      }
   }
   return 1;
}

/* envoie la commande et retient son nom, pour expliquer une éventuelle erreur */
static void envoyerCommande(EtatClient *e, const char *commande, const char *args)
{
   snprintf(e->derniereCommande, sizeof e->derniereCommande, "%s", commande);
   if (args[0] != '\0')
   {
      envoyerAuServeur(e, "%s %s", commande, args);
   }
   else
   {
      envoyerAuServeur(e, "%s", commande);
   }
}

/* ------------------------------------------------------------------ aide */

static void ligneAide(const char *commande, const char *description)
{
   printf("    %s%-22s%s %s\n", coul(C_GRAS), commande, coul(C_RESET), description);
}

static void afficherAide(const EtatClient *e)
{
   titre("Aide");
   if (e->partie != 0)
   {
      alerte("Vous êtes en partie : tapez un chiffre de 1 à 6 pour jouer la case correspondante.");
   }
   else if (e->defiDe[0] != '\0')
   {
      alerte("%s vous a défié : tapez o pour accepter, n pour refuser.", e->defiDe);
   }
   else if (e->partieObservee != 0)
   {
      alerte("Vous regardez la partie #%d : arreter pour en sortir.", e->partieObservee);
   }

   printf("\n  %sJouer%s\n", coul(C_CYAN), coul(C_RESET));
   ligneAide("defier <pseudo>", "lancer un défi (o / n pour répondre à un défi reçu)");
   ligneAide("1 à 6", "pendant une partie : jouer cette case de votre rangée");
   ligneAide("abandonner", "abandonner la partie en cours");
   ligneAide("annuler", "retirer le défi que vous avez lancé");

   printf("\n  %sJoueurs%s\n", coul(C_CYAN), coul(C_RESET));
   ligneAide("joueurs", "qui est en ligne");
   ligneAide("classement [pseudo]", "classement Elo");
   ligneAide("bio [pseudo]", "lire une bio (la vôtre par défaut)");
   ligneAide("bioajout <texte>", "ajouter une ligne à votre bio · bioeffacer pour la vider");

   printf("\n  %sDiscuter%s\n", coul(C_CYAN), coul(C_RESET));
   ligneAide("@pseudo <message>", "message privé (marche aussi pendant une partie)");
   ligneAide("tous <message>", "message à tous les joueurs");

   printf("\n  %sRegarder%s\n", coul(C_CYAN), coul(C_RESET));
   ligneAide("parties", "parties en cours");
   ligneAide("observer <numéro>", "regarder une partie · arreter pour en sortir");
   ligneAide("historique [pseudo]", "10 dernières parties terminées");
   ligneAide("revoir <numéro>", "rejouer une partie terminée coup par coup");

   printf("\n  %sAmis%s\n", coul(C_CYAN), coul(C_RESET));
   ligneAide("ami <pseudo>", "il pourra regarder vos parties privées · plusami pour retirer");
   ligneAide("amis", "votre liste d'amis");
   ligneAide("prive on | off", "parties visibles par vos amis seulement, ou par tous");

   printf("\n");
   ligneAide("quitter", "quitter le jeu");
   info("Les commandes du protocole (PLAY 3, CHALLENGE bob...) marchent aussi.");
}

/* ------------------------------------------------------------------ rejeu */

/* touches du rejeu ; renvoie 0 si la ligne n'en est pas une (c'est alors une commande normale) */
static int naviguerRejeu(EtatClient *e, const char *ligne)
{
   Rejeu *r = &e->rejeu;
   if (ligne[0] == '\0' || strcmp(ligne, "s") == 0 || strcmp(ligne, "suivant") == 0)
   {
      if (r->courant < r->nb - 1)
      {
         r->courant++;
         afficherPositionRejeu(e);
      }
      else
      {
         info("C'est la fin de la partie : q pour quitter le rejeu, p pour revenir en arrière.");
      }
   }
   else if (strcmp(ligne, "p") == 0 || strcmp(ligne, "precedent") == 0)
   {
      if (r->courant > 0)
      {
         r->courant--;
         afficherPositionRejeu(e);
      }
   }
   else if (strcmp(ligne, "d") == 0 || strcmp(ligne, "debut") == 0)
   {
      r->courant = 0;
      afficherPositionRejeu(e);
   }
   else if (strcmp(ligne, "f") == 0 || strcmp(ligne, "fin") == 0)
   {
      r->courant = r->nb - 1;
      afficherPositionRejeu(e);
   }
   else if (strcmp(ligne, "q") == 0 || strcmp(ligne, "sortir") == 0)
   {
      r->actif = 0;
      info("Fin du rejeu.");
   }
   else
   {
      return 0;
   }
   return 1;
}

/* ------------------------------------------------------------------ saisie */

/* une commande dangereuse doit être tapée deux fois de suite.
   attente : la commande qui attendait d'être confirmée avant cette saisie */
static int confirme(EtatClient *e, const char *attente, const char *commande, const char *question)
{
   if (strcmp(attente, commande) == 0)
   {
      return 1;
   }
   snprintf(e->confirmation, sizeof e->confirmation, "%s", commande);
   alerte("%s", question);
   return 0;
}

static void choisirPseudo(EtatClient *e, const char *mot, const char *reste)
{
   const char *pseudo = mot;
   if (strcmp(mot, "login") == 0) /* "login bob" marche aussi */
   {
      pseudo = reste;
   }
   else if (reste[0] != '\0')
   {
      echec("Un pseudo ne contient pas d'espace. Choisissez-en un autre :");
      return;
   }
   envoyerCommande(e, "LOGIN", pseudo);
}

void traiterSaisie(EtatClient *e, char *ligne)
{
   /* enlève le '\n' (et les espaces) de la fin, puis les espaces du début */
   int n = strlen(ligne);
   while (n > 0 && isspace((unsigned char)ligne[n - 1]))
   {
      ligne[--n] = '\0';
   }
   while (isspace((unsigned char)*ligne))
   {
      ligne++;
   }

   if (e->rejeu.actif && e->rejeu.complet)
   {
      if (naviguerRejeu(e, ligne))
      {
         return;
      }
      e->rejeu.actif = 0; /* une autre commande : on sort du rejeu et on la traite normalement */
   }
   if (ligne[0] == '\0')
   {
      return;
   }

   /* coupe "mot reste de la ligne" ; mot est mis en minuscules pour la recherche */
   char mot[BUF_SIZE];
   snprintf(mot, sizeof mot, "%s", ligne);
   char *reste = strchr(mot, ' ');
   if (reste != NULL)
   {
      *reste = '\0';
      reste++;
      while (*reste == ' ')
      {
         reste++;
      }
   }
   else
   {
      reste = "";
   }
   char motMin[BUF_SIZE];
   snprintf(motMin, sizeof motMin, "%s", mot);
   for (int k = 0; motMin[k] != '\0'; k++)
   {
      motMin[k] = tolower((unsigned char)motMin[k]);
   }

   /* une confirmation ne vaut que pour la saisie qui suit : on la retire tout de suite,
      confirme() la remet si besoin */
   char attente[16];
   snprintf(attente, sizeof attente, "%s", e->confirmation);
   e->confirmation[0] = '\0';

   if (estDansListe(motMin, AIDE))
   {
      afficherAide(e);
      return;
   }
   if (strcmp(motMin, "quitter") == 0 || strcmp(motMin, "quit") == 0 || strcmp(motMin, "exit") == 0)
   {
      if (e->partie != 0 && !confirme(e, attente, "QUIT", "Quitter maintenant = abandonner la partie. Retapez quitter pour confirmer."))
      {
         return;
      }
      e->quitte = 1;
      envoyerCommande(e, "QUIT", "");
      return;
   }
   if (!e->identifie)
   {
      choisirPseudo(e, motMin, reste);
      return;
   }
   /* raccourcis */
   if (mot[0] == '@')
   {
      if (mot[1] == '\0' || reste[0] == '\0')
      {
         echec("Pour un message privé : @pseudo votre message");
         return;
      }
      snprintf(e->msgDest, sizeof e->msgDest, "%s", mot + 1);
      snprintf(e->msgTexte, sizeof e->msgTexte, "%s", reste);
      char args[BUF_SIZE];
      snprintf(args, sizeof args, "%s %s", e->msgDest, reste);
      envoyerCommande(e, "MSG", args);
      return;
   }
   if (queDesChiffres(mot) && reste[0] == '\0')
   {
      if (e->partie == 0)
      {
         echec("Les chiffres servent à jouer pendant une partie. Lancez-en une : defier <pseudo>");
         return;
      }
      envoyerCommande(e, "PLAY", mot);
      return;
   }
   if (e->defiDe[0] != '\0' && estDansListe(motMin, OUI))
   {
      envoyerCommande(e, "ACCEPT", "");
      return;
   }
   if (e->defiDe[0] != '\0' && estDansListe(motMin, NON))
   {
      envoyerCommande(e, "REFUSE", "");
      return;
   }

   /* mot français -> commande du protocole ; sinon le mot lui-même, en majuscules */
   char commande[BUF_SIZE];
   snprintf(commande, sizeof commande, "%s", motMin);
   for (int k = 0; commande[k] != '\0'; k++)
   {
      commande[k] = toupper((unsigned char)commande[k]);
   }
   for (int k = 0; k < NB_ALIAS; k++)
   {
      if (strcmp(motMin, ALIAS[k].mot) == 0)
      {
         snprintf(commande, sizeof commande, "%s", ALIAS[k].commande);
         break;
      }
   }

   char args[BUF_SIZE];
   snprintf(args, sizeof args, "%s", reste);

   if (strcmp(commande, "FORFEIT") == 0 && e->partie != 0 &&
       !confirme(e, attente, "FORFEIT", "Abandonner la partie ? Retapez abandonner pour confirmer."))
   {
      return;
   }
   if (strcmp(commande, "PRIVATE") == 0)
   {
      /* "prive on", "prive oui", "PRIVATE ON"... -> ON / OFF */
      for (int k = 0; args[k] != '\0'; k++)
      {
         args[k] = tolower((unsigned char)args[k]);
      }
      if (estDansListe(args, OUI) || strcmp(args, "on") == 0)
      {
         strcpy(args, "ON");
      }
      else if (estDansListe(args, NON) || strcmp(args, "off") == 0)
      {
         strcpy(args, "OFF");
      }
   }
   if (strcmp(commande, "MSG") == 0)
   {
      /* "msg bob salut" : on retient le message pour l'afficher quand le serveur confirme */
      sscanf(args, "%20s", e->msgDest);
      const char *t = strchr(args, ' ');
      snprintf(e->msgTexte, sizeof e->msgTexte, "%s", t != NULL ? t + 1 : "");
   }
   if (strcmp(commande, "ALL") == 0)
   {
      snprintf(e->msgTexte, sizeof e->msgTexte, "%s", args);
   }
   if (strcmp(commande, "WATCH") == 0)
   {
      /* on demande d'abord la liste des parties, sans l'afficher, pour connaître les noms des joueurs */
      e->gamesMuets++;
      envoyerAuServeur(e, "GAMES");
   }
   envoyerCommande(e, commande, args);
}
