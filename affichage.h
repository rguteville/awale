#ifndef AFFICHAGE_H
#define AFFICHAGE_H

/* Tout ce que le client écrit à l'écran : couleurs, plateau, invite, messages */

#include "client.h"

enum Couleur
{
   C_RESET,
   C_GRAS,
   C_PALE,
   C_ROUGE,
   C_VERT,
   C_JAUNE,
   C_BLEU,
   C_MAGENTA,
   C_CYAN,
   C_INVERSE,
   NB_COULEURS
};

/* à appeler une fois au démarrage : couleurs seulement dans un vrai terminal, et pas si NO_COLOR est défini */
void initAffichage(void);

/* code de la couleur, ou "" si les couleurs sont désactivées : printf("%sbonjour%s", coul(C_VERT), coul(C_RESET)) */
const char *coul(enum Couleur c);

void afficherBanniere(const char *adresse, int port);
void afficherInvite(const EtatClient *e); /* "alice › " selon la situation */
void effacerInvite(void);                 /* avant d'écrire un message qui arrive pendant que l'invite est affichée */
void sonnerie(void);                      /* bip du terminal : défi reçu, à vous de jouer */

/* une ligne de message, comme printf, précédée d'un symbole coloré */
void succes(const char *format, ...); /* ✔ vert */
void echec(const char *format, ...);  /* ✖ rouge */
void info(const char *format, ...);   /* • pâle */
void alerte(const char *format, ...); /* ! jaune */
void titre(const char *format, ...);  /* ── titre ───── */
void texte(const char *format, ...);  /* simplement indenté */

/* dessine le plateau. avant : position précédente (NULL s'il n'y en a pas) pour colorer le coup joué
   et les captures. monCamp : 0 ou 1 pour un joueur, -1 pour un observateur */
void afficherPlateau(const struct Jeu *jeu, const struct Jeu *avant, char noms[2][PSEUDO_MAX + 1], int monCamp);

/* case (0 à 11) jouée pour passer de avant à apres, ou -1 si on ne la trouve pas */
int coupJoue(const struct Jeu *avant, const struct Jeu *apres);

/* numéro 1 à 6 affiché au-dessus / au-dessous d'une case : celui qu'on tape pour la jouer */
int numeroAffiche(int numCase);

#endif
