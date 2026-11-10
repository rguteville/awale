#ifndef AWALE_H          /* garde : évite d'inclure le fichier deux fois */
#define AWALE_H

#include <stddef.h>      /* pour size_t */

#define NB_CASES     12
#define GRAINES_INIT 4
#define NB_COUPS_MAX 50

struct Jeu {
    int plateau[NB_CASES];   /* nombre de graines par case, 0-5 : joueur 0, 6-11 : joueur 1 */
    int score[2];            /* graines capturées par chaque joueur */
    int joueurCourant;       /* 0 ou 1 */
    int fini;                /* 0 tant que la partie continue, 1 quand elle est terminée */
    int nbCoups;
};

/* met le jeu dans l'état de départ */
void initJeu(struct Jeu *jeu, int premierJoueur);

/* renvoie 1 si le joueur courant peut jouer la case numCase (0-11), 0 sinon */
int coupLegal(const struct Jeu *jeu, int numCaseCliquee);

/* joue le coup : renvoie 0 si ok, -1 si le coup est illégal (et le jeu n'est pas modifié) */
int jouerCoup(struct Jeu *jeu, int numCaseCliquee);

/* à appeler quand jeu->fini vaut 1 : renvoie 0 ou 1 (gagnant), ou -1 en cas d'égalité */
int gagnant(const struct Jeu *jeu);

/* écrit le plateau et les scores dans buffer (taille = taille du buffer) */
void plateauEnTexte(const struct Jeu *jeu, char *buffer, size_t taille);

#endif
