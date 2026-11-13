#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "messages.h"
#include "affichage.h"

/* ---------------------------------------------------------------------------
   Une fonction par message du serveur, et une table qui associe le début de la
   ligne à sa fonction. Ajouter un message = écrire sa fonction + une ligne de table.
   Chaque fonction reçoit la suite de la ligne (après le mot-clé) dans args.
   --------------------------------------------------------------------------- */

/* lit "<id> <c0> ... <c11> <score0> <score1> <joueur_courant>" ; renvoie 1 si tout y est */
static int lirePlateau(char *args, int *id, struct Jeu *jeu)
{
   char *mot = strtok(args, " ");
   if (mot == NULL)
   {
      return 0;
   }
   *id = atoi(mot);
   int valeurs[NB_CASES + 3];
   for (int k = 0; k < NB_CASES + 3; k++)
   {
      mot = strtok(NULL, " ");
      if (mot == NULL)
      {
         return 0;
      }
      valeurs[k] = atoi(mot);
   }
   initJeu(jeu, 0);
   for (int k = 0; k < NB_CASES; k++)
   {
      jeu->plateau[k] = valeurs[k];
   }
   jeu->score[0] = valeurs[NB_CASES];
   jeu->score[1] = valeurs[NB_CASES + 1];
   jeu->joueurCourant = valeurs[NB_CASES + 2];
   return 1;
}

/* la partie est-elle finie sur ce plateau ? (pour ne pas écrire "à vous de jouer" sur le dernier) */
static int plateauFinal(const struct Jeu *jeu)
{
   if (jeu->score[0] > 24 || jeu->score[1] > 24)
   {
      return 1;
   }
   for (int c = 0; c < NB_CASES; c++)
   {
      if (coupLegal(jeu, c))
      {
         return 0;
      }
   }
   return 1;
}

/* "bob joue la case 3 et capture 4 graines !" */
static void decrireCoup(const struct Jeu *avant, const struct Jeu *apres, char noms[2][PSEUDO_MAX + 1], int monCamp)
{
   int c = coupJoue(avant, apres);
   if (c < 0)
   {
      return;
   }
   int joueur = avant->joueurCourant;
   int captures = apres->score[joueur] - avant->score[joueur];
   char qui[PSEUDO_MAX + 16];
   if (joueur == monCamp)
   {
      snprintf(qui, sizeof qui, "Vous jouez");
   }
   else
   {
      snprintf(qui, sizeof qui, "%s joue", noms[joueur]);
   }
   if (captures > 0)
   {
      printf("  %s%s la case %d et capture %s%d graine%s%s !\n", coul(C_GRAS), qui, numeroAffiche(c),
             coul(C_JAUNE), captures, captures > 1 ? "s" : "", coul(C_RESET));
   }
   else
   {
      info("%s la case %d.", qui, numeroAffiche(c));
   }
}

static void nomsDeLaPartie(EtatClient *e, int id)
{
   for (int k = 0; k < e->nbConnues; k++)
   {
      if (e->connues[k].id == id)
      {
         memcpy(e->noms, e->connues[k].noms, sizeof e->noms);
         return;
      }
   }
   strcpy(e->noms[0], "camp du bas");
   strcpy(e->noms[1], "camp du haut");
}

/* ------------------------------------------------------------------ connexion */

static void okLogin(EtatClient *e, char *args)
{
   if (sscanf(args, "%20s", e->monPseudo) != 1)
   {
      return;
   }
   e->identifie = 1;
   if (e->dejaConnecte)
   {
      succes("Connexion rétablie : vous êtes %s%s%s.", coul(C_GRAS), e->monPseudo, coul(C_RESET));
      return;
   }
   e->dejaConnecte = 1;
   printf("\n");
   succes("Bienvenue %s%s%s !", coul(C_GRAS), e->monPseudo, coul(C_RESET));
   /* l'accueil : qui est en ligne, et mon Elo (affichés en version courte) */
   e->accueil = 2;
   envoyerAuServeur(e, "LIST");
   envoyerAuServeur(e, "RANK %s", e->monPseudo);
}

static void users(EtatClient *e, char *args)
{
   char liste[BUF_SIZE] = "";
   int n = 0;
   for (char *p = strtok(args, " "); p != NULL; p = strtok(NULL, " "))
   {
      if (strcmp(p, e->monPseudo) == 0)
      {
         continue;
      }
      if (n > 0)
      {
         strncat(liste, ", ", sizeof liste - strlen(liste) - 1);
      }
      strncat(liste, p, sizeof liste - strlen(liste) - 1);
      n++;
   }
   if (e->accueil > 0)
   {
      e->accueil--;
      if (n == 0)
      {
         info("Personne d'autre n'est connecté pour l'instant.");
      }
      else
      {
         info("En ligne : %s", liste);
      }
      if (e->accueil == 0 && e->partie == 0)
      {
         texte("Tapez %saide%s pour voir les commandes, ou %sdefier <pseudo>%s pour lancer une partie.",
               coul(C_GRAS), coul(C_RESET), coul(C_GRAS), coul(C_RESET));
      }
      return;
   }
   if (n == 0)
   {
      info("Personne d'autre n'est en ligne pour l'instant.");
   }
   else
   {
      titre("Joueurs en ligne (%d en plus de vous)", n);
      texte("%s", liste);
      info("defier <pseudo> pour lancer une partie");
   }
}

/* ------------------------------------------------------------------ défis */

static void okChallenge(EtatClient *e, char *args)
{
   sscanf(args, "%20s", e->defiA);
   succes("Défi envoyé à %s%s%s. En attente de sa réponse…", coul(C_GRAS), e->defiA, coul(C_RESET));
   info("annuler pour retirer votre défi");
}

static void challenged(EtatClient *e, char *args)
{
   sscanf(args, "%20s", e->defiDe);
   sonnerie();
   printf("\n  %s%s» %s vous défie ! «%s\n", coul(C_GRAS), coul(C_JAUNE), e->defiDe, coul(C_RESET));
   texte("Tapez %so%s pour accepter, %sn%s pour refuser.", coul(C_GRAS), coul(C_RESET), coul(C_GRAS), coul(C_RESET));
}

static void okRefuse(EtatClient *e, char *args)
{
   (void)args;
   info("Vous avez refusé le défi de %s.", e->defiDe);
   e->defiDe[0] = '\0';
}

static void okCancel(EtatClient *e, char *args)
{
   (void)args;
   info("Votre défi à %s est annulé.", e->defiA);
   e->defiA[0] = '\0';
}

static void refused(EtatClient *e, char *args)
{
   (void)args;
   echec("%s a refusé votre défi.", e->defiA);
   e->defiA[0] = '\0';
}

static void cancelled(EtatClient *e, char *args)
{
   char qui[PSEUDO_MAX + 1] = "";
   sscanf(args, "%20s", qui);
   if (strcmp(qui, e->defiDe) == 0)
   {
      info("%s a retiré son défi.", qui);
      e->defiDe[0] = '\0';
   }
   else
   {
      info("Le défi avec %s est annulé (déconnexion).", qui);
      e->defiA[0] = '\0';
   }
}

/* ------------------------------------------------------------------ partie */

static void debutPartie(EtatClient *e, int id, const char *adv, int camp)
{
   e->partie = id;
   e->monCamp = camp;
   snprintf(e->adversaire, sizeof e->adversaire, "%s", adv);
   snprintf(e->noms[camp], sizeof e->noms[camp], "%s", e->monPseudo);
   snprintf(e->noms[1 - camp], sizeof e->noms[1 - camp], "%s", adv);
   e->defiA[0] = '\0';
   e->defiDe[0] = '\0';
   e->partieObservee = 0;
   e->enPause = 0;
   e->aDernier = 0;
}

static void start(EtatClient *e, char *args)
{
   int id, camp;
   char adv[PSEUDO_MAX + 1];
   if (sscanf(args, "%d %20s %d", &id, adv, &camp) != 3)
   {
      return;
   }
   debutPartie(e, id, adv, camp);
   sonnerie();
   titre("Partie #%d : vous contre %s", id, adv);
   texte("Vous jouez la rangée du %s%s%s : tapez le numéro (1 à 6) affiché %s la case.",
         coul(C_GRAS), camp == 0 ? "bas" : "haut", coul(C_RESET), camp == 0 ? "sous" : "au-dessus de");
   if (!e->reglesVues)
   {
      e->reglesVues = 1;
      info("Règles : on sème les graines d'une case une par une, dans le sens inverse des aiguilles");
      info("d'une montre. Si la dernière tombe chez l'adversaire et y fait 2 ou 3 graines, on les capture");
      info("(et celles des cases précédentes dans le même cas). Interdit d'affamer l'adversaire.");
   }
   if (camp == 0)
   {
      succes("Le tirage au sort vous désigne : c'est vous qui commencez !");
   }
   else
   {
      info("Le tirage au sort a choisi : %s commence.", adv);
   }
}

static void resume(EtatClient *e, char *args)
{
   int id, camp;
   char adv[PSEUDO_MAX + 1];
   if (sscanf(args, "%d %20s %d", &id, adv, &camp) != 3)
   {
      return;
   }
   debutPartie(e, id, adv, camp);
   succes("Vous revoilà dans la partie #%d contre %s, là où vous l'aviez laissée !", id, adv);
}

static void resumed(EtatClient *e, char *args)
{
   int id;
   char qui[PSEUDO_MAX + 1];
   if (sscanf(args, "%d %20s", &id, qui) != 2)
   {
      return;
   }
   if (e->partie == id)
   {
      e->enPause = 0;
   }
   succes("%s est de retour : la partie #%d reprend !", qui, id);
}

static void pauseRecue(EtatClient *e, char *args)
{
   int id, secondes;
   char qui[PSEUDO_MAX + 1];
   if (sscanf(args, "%d %20s %d", &id, qui, &secondes) != 3)
   {
      return;
   }
   if (e->partie == id)
   {
      e->enPause = 1;
      alerte("%s a perdu la connexion : partie en pause. Sans retour d'ici %d s, vous gagnez.", qui, secondes);
   }
   else
   {
      alerte("%s a perdu la connexion : partie #%d en pause (%d s pour revenir).", qui, id, secondes);
   }
}

static void board(EtatClient *e, char *args)
{
   int id;
   struct Jeu jeu;
   if (!lirePlateau(args, &id, &jeu))
   {
      return;
   }

   /* rejeu : les positions arrivent toutes d'un coup, on les garde pour les montrer une par une */
   Rejeu *r = &e->rejeu;
   if (r->actif && !r->complet)
   {
      if (r->nb < NB_COUPS_MAX + 1)
      {
         r->positions[r->nb] = jeu;
         r->nb++;
      }
      return;
   }

   int monCamp = (e->partie == id) ? e->monCamp : -1;
   const struct Jeu *avant = e->aDernier ? &e->dernier : NULL;
   afficherPlateau(&jeu, avant, e->noms, monCamp);
   if (avant != NULL)
   {
      decrireCoup(avant, &jeu, e->noms, monCamp);
   }
   e->dernier = jeu;
   e->aDernier = 1;

   if (plateauFinal(&jeu))
   {
      return; /* END arrive juste après */
   }
   if (e->partie == id && jeu.joueurCourant == e->monCamp)
   {
      sonnerie();
      printf("  %s%s▶ À vous de jouer !%s Tapez un chiffre de 1 à 6.\n", coul(C_GRAS), coul(C_VERT), coul(C_RESET));
   }
   else if (e->partie == id)
   {
      info("Au tour de %s…", e->adversaire);
   }
   else
   {
      info("Au tour de %s.", e->noms[jeu.joueurCourant]);
   }
}

/* raison de la fin, du point de vue de celui qui lit */
static void expliquerFin(const char *raison, const char *gagnant, const char *perdant, int jeSuisGagnant, int jeSuisPerdant)
{
   if (strcmp(raison, "SCORE") == 0)
   {
      texte("Plus de 24 graines capturées.");
   }
   else if (strcmp(raison, "BLOQUE") == 0)
   {
      texte("Plus aucun coup possible : le plus de graines capturées l'emporte.");
   }
   else if (strcmp(raison, "LIMITE") == 0)
   {
      texte("Limite de %d coups atteinte : le plus de graines capturées l'emporte.", NB_COUPS_MAX);
   }
   else if (strcmp(raison, "ABANDON") == 0)
   {
      if (strcmp(gagnant, "NUL") == 0)
      {
         texte("Abandon.");
      }
      else if (jeSuisPerdant)
      {
         texte("Vous avez abandonné.");
      }
      else
      {
         texte("%s a abandonné.", perdant);
      }
   }
   else if (strcmp(raison, "DECONNEXION") == 0)
   {
      if (strcmp(gagnant, "NUL") == 0)
      {
         texte("Les deux joueurs se sont déconnectés.");
      }
      else if (jeSuisGagnant)
      {
         texte("%s n'a pas pu revenir à temps.", perdant);
      }
      else if (jeSuisPerdant)
      {
         texte("Votre connexion a été coupée trop longtemps.");
      }
      else
      {
         texte("%s n'a pas pu revenir à temps.", perdant);
      }
   }
}

static void end(EtatClient *e, char *args)
{
   int id;
   char gagnant[PSEUDO_MAX + 1], raison[16];
   if (sscanf(args, "%d %20s %15s", &id, gagnant, raison) != 3)
   {
      return;
   }

   Rejeu *r = &e->rejeu;
   if (r->actif && !r->complet)
   {
      /* toutes les positions du rejeu sont arrivées : on montre la première */
      snprintf(r->gagnant, sizeof r->gagnant, "%s", gagnant);
      snprintf(r->raison, sizeof r->raison, "%s", raison);
      r->complet = 1;
      r->courant = 0;
      titre("Rejeu de la partie #%d : %s contre %s (%d coups)", r->id, r->noms[0], r->noms[1], r->nb - 1);
      info("Entrée : coup suivant · p : précédent · d : début · f : fin · q : quitter le rejeu");
      afficherPositionRejeu(e);
      return;
   }

   int joueur = (e->partie == id);
   int nul = strcmp(gagnant, "NUL") == 0;
   int gagne = joueur && strcmp(gagnant, e->monPseudo) == 0;
   const char *perdant = "";
   if (!nul)
   {
      perdant = (strcmp(e->noms[0], gagnant) == 0) ? e->noms[1] : e->noms[0];
   }

   printf("\n");
   if (joueur && nul)
   {
      printf("  %s%s═══  MATCH NUL  ═══%s\n", coul(C_GRAS), coul(C_JAUNE), coul(C_RESET));
   }
   else if (gagne)
   {
      printf("  %s%s★ ★ ★  VICTOIRE !  ★ ★ ★%s\n", coul(C_GRAS), coul(C_VERT), coul(C_RESET));
   }
   else if (joueur)
   {
      printf("  %s%s═══  DÉFAITE  ═══%s\n", coul(C_GRAS), coul(C_ROUGE), coul(C_RESET));
   }
   else if (nul)
   {
      printf("  %sPartie #%d terminée : match nul.%s\n", coul(C_GRAS), id, coul(C_RESET));
   }
   else
   {
      printf("  %sPartie #%d terminée : victoire de %s.%s\n", coul(C_GRAS), id, gagnant, coul(C_RESET));
   }
   expliquerFin(raison, gagnant, perdant, gagne, joueur && !gagne && !nul);
   if (e->aDernier)
   {
      texte("Score final : %s %d · %s %d", e->noms[0], e->dernier.score[0], e->noms[1], e->dernier.score[1]);
   }

   e->dernierePartie = id;
   if (!joueur)
   {
      info("revoir %d pour revoir la partie coup par coup", id);
   }
   /* le serveur nous a remis LIBRE */
   e->partie = 0;
   e->monCamp = -1;
   e->enPause = 0;
   e->partieObservee = 0;
   e->aDernier = 0;
}

static void elo(EtatClient *e, char *args)
{
   int ancien, nouveau;
   if (sscanf(args, "%d %d", &ancien, &nouveau) != 2)
   {
      return;
   }
   int diff = nouveau - ancien;
   printf("    Elo : %d → %s%s%d (%+d)%s\n", ancien, coul(C_GRAS), coul(diff >= 0 ? C_VERT : C_ROUGE),
          nouveau, diff, coul(C_RESET));
   info("revoir %d pour revoir la partie · defier <pseudo> pour rejouer · classement", e->dernierePartie);
}

/* ------------------------------------------------------------------ observation et rejeu */

static void games(EtatClient *e, char *args)
{
   e->nbConnues = 0;
   for (char *p = strtok(args, " "); p != NULL && e->nbConnues < MAX_PARTIES_CONNUES; p = strtok(NULL, " "))
   {
      PartieConnue *c = &e->connues[e->nbConnues];
      /* %20[^:] : jusqu'à 20 caractères qui ne sont pas ':' */
      if (sscanf(p, "%d:%20[^:]:%20s", &c->id, c->noms[0], c->noms[1]) == 3)
      {
         e->nbConnues++;
      }
   }
   if (e->gamesMuets > 0) /* demandé par le client lui-même avant un WATCH : on range sans afficher */
   {
      e->gamesMuets--;
      return;
   }
   if (e->nbConnues == 0)
   {
      info("Aucune partie en cours. Lancez-en une : defier <pseudo>");
      return;
   }
   titre("Parties en cours");
   for (int k = 0; k < e->nbConnues; k++)
   {
      texte("%s#%-3d%s %s contre %s", coul(C_GRAS), e->connues[k].id, coul(C_RESET), e->connues[k].noms[0], e->connues[k].noms[1]);
   }
   info("observer <numéro> pour en regarder une");
}

static void okWatch(EtatClient *e, char *args)
{
   e->partieObservee = atoi(args);
   e->aDernier = 0;
   nomsDeLaPartie(e, e->partieObservee);
   titre("Vous regardez la partie #%d : %s contre %s", e->partieObservee, e->noms[0], e->noms[1]);
   info("arreter pour ne plus regarder");
}

static void okUnwatch(EtatClient *e, char *args)
{
   (void)args;
   info("Vous ne regardez plus la partie #%d.", e->partieObservee);
   e->partieObservee = 0;
   e->aDernier = 0;
}

static void okReplay(EtatClient *e, char *args)
{
   Rejeu *r = &e->rejeu;
   if (sscanf(args, "%d %20s %20s", &r->id, r->noms[0], r->noms[1]) != 3)
   {
      return;
   }
   r->actif = 1;
   r->complet = 0;
   r->nb = 0;
   r->courant = 0;
}

void afficherPositionRejeu(EtatClient *e)
{
   Rejeu *r = &e->rejeu;
   if (r->nb == 0)
   {
      echec("Rien à rejouer.");
      r->actif = 0;
      return;
   }
   const struct Jeu *jeu = &r->positions[r->courant];
   const struct Jeu *avant = (r->courant > 0) ? &r->positions[r->courant - 1] : NULL;
   if (r->courant == 0)
   {
      info("Position de départ (%s commence)", r->noms[0]);
   }
   else
   {
      info("Coup %d sur %d", r->courant, r->nb - 1);
   }
   afficherPlateau(jeu, avant, r->noms, -1);
   if (avant != NULL)
   {
      decrireCoup(avant, jeu, r->noms, -1);
   }
   if (r->courant == r->nb - 1)
   {
      if (strcmp(r->gagnant, "NUL") == 0)
      {
         printf("  %sFin : match nul.%s\n", coul(C_GRAS), coul(C_RESET));
      }
      else
      {
         printf("  %sFin : victoire de %s.%s\n", coul(C_GRAS), r->gagnant, coul(C_RESET));
      }
      const char *perdant = (strcmp(r->noms[0], r->gagnant) == 0) ? r->noms[1] : r->noms[0];
      expliquerFin(r->raison, r->gagnant, perdant, 0, 0);
      info("q pour quitter le rejeu · p pour revenir en arrière");
   }
}

/* ------------------------------------------------------------------ chat */

static void okMsg(EtatClient *e, char *args)
{
   (void)args;
   printf("  %s[privé] vous → %s :%s %s\n", coul(C_MAGENTA), e->msgDest, coul(C_RESET), e->msgTexte);
}

static void okAll(EtatClient *e, char *args)
{
   (void)args;
   printf("  %s[tous] vous :%s %s\n", coul(C_BLEU), coul(C_RESET), e->msgTexte);
}

static void msg(EtatClient *e, char *args)
{
   (void)e;
   char de[PSEUDO_MAX + 1], t[BUF_SIZE];
   if (sscanf(args, "%20s %1023[^\n]", de, t) == 2)
   {
      printf("  %s%s[privé] %s :%s %s\n", coul(C_GRAS), coul(C_MAGENTA), de, coul(C_RESET), t);
      info("répondre : @%s <message>", de);
   }
}

static void all(EtatClient *e, char *args)
{
   (void)e;
   char de[PSEUDO_MAX + 1], t[BUF_SIZE];
   if (sscanf(args, "%20s %1023[^\n]", de, t) == 2)
   {
      printf("  %s[tous] %s :%s %s\n", coul(C_BLEU), de, coul(C_RESET), t);
   }
}

/* ------------------------------------------------------------------ bio */

static void bioline(EtatClient *e, char *args)
{
   char qui[PSEUDO_MAX + 1], t[BUF_SIZE];
   if (sscanf(args, "%20s %1023[^\n]", qui, t) != 2)
   {
      return;
   }
   if (e->bioLignes == 0)
   {
      titre("Bio de %s", qui);
   }
   texte("%s", t);
   e->bioLignes++;
}

static void bioend(EtatClient *e, char *args)
{
   char qui[PSEUDO_MAX + 1] = "";
   sscanf(args, "%20s", qui);
   if (e->bioLignes == 0)
   {
      if (strcmp(qui, e->monPseudo) == 0)
      {
         info("Vous n'avez pas encore de bio : bioajout <texte> pour en écrire une ligne.");
      }
      else
      {
         info("%s n'a pas encore écrit de bio.", qui);
      }
   }
   e->bioLignes = 0;
}

static void okBioadd(EtatClient *e, char *args)
{
   (void)e;
   (void)args;
   succes("Ligne ajoutée à votre bio (bio pour la voir).");
}

static void okBioclear(EtatClient *e, char *args)
{
   (void)e;
   (void)args;
   succes("Votre bio est effacée.");
}

/* ------------------------------------------------------------------ amis */

static void okFriend(EtatClient *e, char *args)
{
   (void)e;
   succes("%s fait partie de vos amis : il ou elle pourra regarder vos parties privées.", args);
}

static void okUnfriend(EtatClient *e, char *args)
{
   (void)e;
   succes("%s n'est plus dans vos amis.", args);
}

static void okPrivate(EtatClient *e, char *args)
{
   (void)e;
   if (strcmp(args, "ON") == 0)
   {
      succes("Mode privé : seuls vos amis pourront regarder vos prochaines parties.");
   }
   else
   {
      succes("Mode public : tout le monde peut regarder vos parties.");
   }
}

static void friends(EtatClient *e, char *args)
{
   (void)e;
   if (args[0] == '\0')
   {
      info("Pas encore d'amis : ami <pseudo> pour en ajouter.");
      return;
   }
   titre("Vos amis (ils peuvent regarder vos parties privées)");
   texte("%s", args);
}

/* ------------------------------------------------------------------ historique et classement */

static void history(EtatClient *e, char *args)
{
   if (args[0] == '\0')
   {
      info("Aucune partie terminée.");
      return;
   }
   titre("Dernières parties terminées");
   for (char *p = strtok(args, " "); p != NULL; p = strtok(NULL, " "))
   {
      int id;
      char j0[PSEUDO_MAX + 1], j1[PSEUDO_MAX + 1], g[PSEUDO_MAX + 1];
      if (sscanf(p, "%d:%20[^:]:%20[^:]:%20s", &id, j0, j1, g) != 4)
      {
         continue;
      }
      int moi = strcmp(j0, e->monPseudo) == 0 || strcmp(j1, e->monPseudo) == 0;
      const char *style = "";
      char resultat[64];
      if (strcmp(g, "NUL") == 0)
      {
         snprintf(resultat, sizeof resultat, "match nul");
      }
      else
      {
         snprintf(resultat, sizeof resultat, "victoire de %s", g);
         if (moi)
         {
            style = coul(strcmp(g, e->monPseudo) == 0 ? C_VERT : C_ROUGE);
         }
      }
      texte("%s#%-4d%s %s contre %s  →  %s%s%s", coul(C_GRAS), id, coul(C_RESET), j0, j1, style, resultat, coul(C_RESET));
   }
   info("revoir <numéro> pour rejouer une partie coup par coup");
}

static void ranking(EtatClient *e, char *args)
{
   int accueil = 0;
   if (e->accueil > 0)
   {
      e->accueil--;
      accueil = 1;
   }
   if (args[0] == '\0')
   {
      if (accueil)
      {
         info("Pas encore de classement : vous démarrez à 1200 Elo.");
      }
      else
      {
         info("Pas encore de classement ici : jouez une partie !");
      }
   }
   else if (!accueil)
   {
      titre("Classement Elo");
      printf("    %s  #  %-20s   Elo    V    N    D%s\n", coul(C_PALE), "joueur", coul(C_RESET));
   }
   for (char *p = strtok(args, " "); p != NULL; p = strtok(NULL, " "))
   {
      int rang, elo, v, n, d;
      char qui[PSEUDO_MAX + 1];
      if (sscanf(p, "%d:%20[^:]:%d:%d:%d:%d", &rang, qui, &elo, &v, &n, &d) != 6)
      {
         continue;
      }
      if (accueil)
      {
         info("Votre Elo : %d (%de du classement · %d V / %d N / %d D)", elo, rang, v, n, d);
         continue;
      }
      int moi = strcmp(qui, e->monPseudo) == 0;
      printf("    %s%3d  %-20s %5d %4d %4d %4d%s\n", moi ? coul(C_GRAS) : "", rang, qui, elo, v, n, d, coul(C_RESET));
   }
   if (accueil && e->accueil == 0 && e->partie == 0)
   {
      texte("Tapez %saide%s pour voir les commandes, ou %sdefier <pseudo>%s pour lancer une partie.",
            coul(C_GRAS), coul(C_RESET), coul(C_GRAS), coul(C_RESET));
   }
}

/* ------------------------------------------------------------------ divers */

static void infoServeur(EtatClient *e, char *args)
{
   (void)e;
   char qui[PSEUDO_MAX + 1];
   if (sscanf(args, "%20s s'est connecte", qui) == 1 && strstr(args, "s'est connecte") != NULL)
   {
      info("%s vient d'arriver.", qui);
   }
   else if (sscanf(args, "%20s s'est deconnecte", qui) == 1 && strstr(args, "s'est deconnecte") != NULL)
   {
      info("%s vient de partir.", qui);
   }
   else if (strstr(args, "sauvegarde abime") != NULL)
   {
      alerte("La sauvegarde de cette partie est abîmée : le rejeu s'arrête ici.");
   }
   else
   {
      info("%s", args);
   }
}

static void helpline(EtatClient *e, char *args)
{
   (void)e;
   texte("%s", args);
}

static void helpend(EtatClient *e, char *args)
{
   (void)e;
   (void)args;
}

static void okQuit(EtatClient *e, char *args)
{
   (void)e;
   (void)args;
   printf("\n");
   succes("À bientôt !");
}

static void okAutre(EtatClient *e, char *args)
{
   (void)e;
   succes("%s", args);
}

/* Les textes d'erreur du serveur parlent le langage du protocole ("usage : FRIEND <pseudo>"),
   sans accents. Le client ne les affiche donc pas tels quels : il explique l'erreur avec ce que
   le joueur tape vraiment ("ami <pseudo>"), grâce à la dernière commande envoyée. */

typedef struct
{
   const char *commande; /* commande du protocole */
   const char *usage;    /* ce que le joueur tape dans ce client */
} Usage;

static const Usage USAGES[] = {
    {"CHALLENGE", "defier <pseudo>"},
    {"PLAY", "un chiffre de 1 à 6 (la case de votre rangée)"},
    {"WATCH", "observer <numéro>   (parties pour voir les numéros)"},
    {"MSG", "@pseudo <message>"},
    {"ALL", "tous <message>"},
    {"BIO", "bio [pseudo]"},
    {"BIOADD", "bioajout <texte>   (100 caractères au plus)"},
    {"FRIEND", "ami <pseudo>   (un joueur connecté, pas vous-même)"},
    {"UNFRIEND", "plusami <pseudo>   (amis pour voir votre liste)"},
    {"PRIVATE", "prive on   ou   prive off"},
    {"HISTORY", "historique [pseudo]"},
    {"REPLAY", "revoir <numéro>   (historique pour voir les numéros)"},
    {"RANK", "classement [pseudo]"},
};

#define NB_USAGES ((int)(sizeof USAGES / sizeof USAGES[0]))

static const char *usageDe(const char *commande)
{
   for (int k = 0; k < NB_USAGES; k++)
   {
      if (strcmp(USAGES[k].commande, commande) == 0)
      {
         return USAGES[k].usage;
      }
   }
   return NULL;
}

/* commande mal écrite (argument manquant ou invalide) */
static void erreurSyntaxe(EtatClient *e, const char *t)
{
   const char *cmd = e->derniereCommande;
   if (!e->identifie)
   {
      echec("Pseudo invalide : 1 à 20 lettres, chiffres ou _ (sans espace ni accent). Choisissez-en un autre :");
   }
   else if (strstr(t, "vous-meme") != NULL && strcmp(cmd, "CHALLENGE") == 0)
   {
      echec("Vous ne pouvez pas vous défier vous-même.");
   }
   else if (strstr(t, "trop longue") != NULL && strcmp(cmd, "BIOADD") == 0)
   {
      echec("Ligne de bio trop longue : %d caractères au plus.", BIO_LIGNE_MAX);
   }
   else if (strstr(t, "trop longue") != NULL)
   {
      echec("Ligne trop longue.");
   }
   else if (strcmp(cmd, "PLAY") == 0)
   {
      echec("Choisissez une case entre 1 et 6.");
   }
   else if (usageDe(cmd) != NULL)
   {
      echec("Format attendu : %s%s%s", coul(C_GRAS), usageDe(cmd), coul(C_RESET));
   }
   else
   {
      echec("Commande mal écrite. Tapez %saide%s pour voir comment l'utiliser.", coul(C_GRAS), coul(C_RESET));
   }
}

/* commande impossible dans la situation actuelle */
static void erreurEtat(EtatClient *e, const char *t)
{
   const char *cmd = e->derniereCommande;
   if (strcmp(cmd, "PLAY") == 0 || strcmp(cmd, "FORFEIT") == 0)
   {
      echec("Vous n'êtes pas en partie. Lancez-en une avec %sdefier <pseudo>%s.", coul(C_GRAS), coul(C_RESET));
   }
   else if ((strcmp(cmd, "ACCEPT") == 0 || strcmp(cmd, "REFUSE") == 0) && strstr(t, "plus valide") != NULL)
   {
      echec("Ce défi n'est plus valable : son auteur est parti.");
   }
   else if (strcmp(cmd, "ACCEPT") == 0 || strcmp(cmd, "REFUSE") == 0)
   {
      echec("Personne ne vous a défié.");
   }
   else if (strcmp(cmd, "CANCEL") == 0)
   {
      echec("Vous n'avez pas lancé de défi.");
   }
   else if (strcmp(cmd, "UNWATCH") == 0)
   {
      echec("Vous ne regardez aucune partie.");
   }
   else if (strcmp(cmd, "LOGIN") == 0)
   {
      echec("Vous êtes déjà connecté sous le pseudo %s.", e->monPseudo);
   }
   else if (strcmp(cmd, "CHALLENGE") == 0 || strcmp(cmd, "WATCH") == 0 || strcmp(cmd, "REPLAY") == 0)
   {
      /* dire précisément ce qui bloque, et comment s'en sortir */
      if (e->partie != 0)
      {
         echec("Pas pendant une partie : finissez-la d'abord (ou abandonner).");
      }
      else if (e->defiDe[0] != '\0')
      {
         echec("%s vous a déjà défié : tapez %so%s pour accepter ou %sn%s pour refuser.", e->defiDe,
               coul(C_GRAS), coul(C_RESET), coul(C_GRAS), coul(C_RESET));
      }
      else if (e->defiA[0] != '\0')
      {
         echec("Vous attendez déjà la réponse de %s : tapez %sannuler%s pour retirer ce défi.", e->defiA,
               coul(C_GRAS), coul(C_RESET));
      }
      else if (e->partieObservee != 0)
      {
         echec("Vous regardez une partie : tapez %sarreter%s d'abord.", coul(C_GRAS), coul(C_RESET));
      }
      else
      {
         echec("Pas maintenant : terminez d'abord votre partie, votre défi ou votre observation.");
      }
   }
   else
   {
      echec("Impossible pour l'instant. Tapez %saide%s pour voir ce que vous pouvez faire.", coul(C_GRAS), coul(C_RESET));
   }
}

/* une erreur du serveur, expliquée avec ce qu'on sait de la situation */
static void err(EtatClient *e, char *args)
{
   char code[16] = "", t[BUF_SIZE] = "";
   sscanf(args, "%15s %1023[^\n]", code, t);
   const char *cmd = e->derniereCommande;
   char qui[PSEUDO_MAX + 1] = "";

   if (strcmp(code, "UNKNOWN") == 0)
   {
      echec("Commande inconnue. Tapez %saide%s pour voir la liste.", coul(C_GRAS), coul(C_RESET));
   }
   else if (strcmp(code, "TAKEN") == 0)
   {
      echec("Ce pseudo est déjà pris. Choisissez-en un autre :");
   }
   else if (strcmp(code, "SYNTAX") == 0)
   {
      erreurSyntaxe(e, t);
   }
   else if (strcmp(code, "STATE") == 0)
   {
      erreurEtat(e, t);
   }
   else if (strcmp(code, "NOTYOURTURN") == 0)
   {
      echec("Patience : c'est à %s de jouer.", e->adversaire);
   }
   else if (strcmp(code, "ILLEGAL") == 0)
   {
      int numero;
      if (sscanf(t, "la case %d est vide", &numero) == 1)
      {
         echec("Coup impossible : la case %d est vide.", numero);
      }
      else
      {
         echec("Coup impossible : il ne resterait plus aucune graine à votre adversaire.");
         info("Règle : on doit toujours lui laisser de quoi jouer. Choisissez une autre case.");
      }
   }
   else if (strcmp(code, "PAUSED") == 0)
   {
      alerte("Partie en pause : attendez le retour de %s.", e->adversaire);
   }
   else if (strcmp(code, "DENIED") == 0)
   {
      echec("Partie privée : seuls les amis des joueurs peuvent la voir.");
   }
   else if (strcmp(code, "BUSY") == 0)
   {
      if (sscanf(t, "%20s n'est pas libre", qui) == 1 && strstr(t, "n'est pas libre") != NULL)
      {
         echec("%s n'est pas disponible (en partie ou déjà en plein défi). Réessayez plus tard.", qui);
      }
      else
      {
         echec("Trop de parties en cours sur le serveur. Réessayez plus tard.");
      }
   }
   else if (strcmp(code, "NOTFOUND") == 0)
   {
      if (strcmp(cmd, "WATCH") == 0)
      {
         echec("Pas de partie en cours avec ce numéro. Tapez %sparties%s pour la liste.", coul(C_GRAS), coul(C_RESET));
      }
      else if (strcmp(cmd, "REPLAY") == 0)
      {
         echec("Pas de partie terminée avec ce numéro. Tapez %shistorique%s.", coul(C_GRAS), coul(C_RESET));
      }
      else if (strcmp(cmd, "UNFRIEND") == 0)
      {
         sscanf(t, "%20s", qui);
         echec("%s ne fait pas partie de vos amis. Tapez %samis%s pour voir la liste.", qui, coul(C_GRAS), coul(C_RESET));
      }
      else
      {
         echec("Aucun joueur connecté ne s'appelle ainsi. Tapez %sjoueurs%s pour voir qui est en ligne.", coul(C_GRAS), coul(C_RESET));
      }
   }
   else if (strcmp(code, "FULL") == 0)
   {
      if (strcmp(cmd, "BIOADD") == 0)
      {
         echec("Votre bio est pleine (%d lignes). Tapez %sbioeffacer%s pour recommencer.", BIO_LIGNES_MAX, coul(C_GRAS), coul(C_RESET));
      }
      else if (strcmp(cmd, "FRIEND") == 0)
      {
         echec("Liste d'amis pleine. Tapez %splusami <pseudo>%s pour faire de la place.", coul(C_GRAS), coul(C_RESET));
      }
      else if (strcmp(cmd, "WATCH") == 0)
      {
         echec("Trop de spectateurs sur cette partie. Réessayez plus tard.");
      }
      else
      {
         echec("Le serveur est plein. Réessayez plus tard.");
      }
   }
   else
   {
      /* code inconnu de ce client (serveur plus récent ?) : on montre le texte du serveur tel quel */
      echec("%s %s", code, t);
   }
}

/* ------------------------------------------------------------------ la table */

typedef void (*FonctionMessage)(EtatClient *e, char *args);

typedef struct
{
   const char *motCle; /* début de la ligne, ex. "OK LOGIN" */
   FonctionMessage fonction;
} Message;

/* les "OK xxx" précis AVANT le "OK" générique : on prend la première ligne qui correspond */
static const Message MESSAGES[] = {
    {"OK LOGIN", okLogin},
    {"OK CHALLENGE", okChallenge},
    {"OK REFUSE", okRefuse},
    {"OK CANCEL", okCancel},
    {"OK WATCH", okWatch},
    {"OK UNWATCH", okUnwatch},
    {"OK REPLAY", okReplay},
    {"OK MSG", okMsg},
    {"OK ALL", okAll},
    {"OK BIOADD", okBioadd},
    {"OK BIOCLEAR", okBioclear},
    {"OK FRIEND", okFriend},
    {"OK UNFRIEND", okUnfriend},
    {"OK PRIVATE", okPrivate},
    {"OK QUIT", okQuit},
    {"OK", okAutre},
    {"ERR", err},
    {"USERS", users},
    {"CHALLENGED", challenged},
    {"REFUSED", refused},
    {"CANCELLED", cancelled},
    {"START", start},
    {"RESUME", resume},
    {"RESUMED", resumed},
    {"PAUSE", pauseRecue},
    {"BOARD", board},
    {"END", end},
    {"ELO", elo},
    {"GAMES", games},
    {"MSG", msg},
    {"ALL", all},
    {"BIOLINE", bioline},
    {"BIOEND", bioend},
    {"FRIENDS", friends},
    {"HISTORY", history},
    {"RANKING", ranking},
    {"INFO", infoServeur},
    {"HELPLINE", helpline},
    {"HELPEND", helpend},
};

#define NB_MESSAGES ((int)(sizeof MESSAGES / sizeof MESSAGES[0]))

void traiterMessage(EtatClient *e, char *ligne)
{
   for (int k = 0; k < NB_MESSAGES; k++)
   {
      int n = strlen(MESSAGES[k].motCle);
      /* le mot-clé doit être un mot entier : "RESUME" ne doit pas attraper "RESUMED ..." */
      if (strncmp(ligne, MESSAGES[k].motCle, n) == 0 && (ligne[n] == ' ' || ligne[n] == '\0'))
      {
         char *args = (ligne[n] == ' ') ? ligne + n + 1 : ligne + n;
         MESSAGES[k].fonction(e, args);
         return;
      }
   }
   printf("  %s\n", ligne); /* message inconnu : on l'affiche tel quel */
}
