#include <stdio.h>
#include "awale.h"

void initJeu(struct Jeu *jeu, int premierJoueur)
{
    for (int i = 0; i < NB_CASES; i++) {
        jeu->plateau[i] = GRAINES_INIT;
    }
    jeu->score[0] = 0;
    jeu->score[1] = 0;
    jeu->joueurCourant = premierJoueur;
    jeu->fini = 0;
    jeu->nbCoups = 0;
}

void plateauEnTexte(const struct Jeu *jeu, char *buffer, size_t taille)
{
    int offset = 0;
    for (int i = 11; i >= 6; i--) {
        offset += snprintf(buffer + offset, taille - offset, "%d ", jeu->plateau[i]);
    }
    offset += snprintf(buffer + offset, taille - offset, "\n");
    for (int i = 0; i < 6; i++) {
        offset += snprintf(buffer + offset, taille - offset, "%d ", jeu->plateau[i]);
    }
    offset += snprintf(buffer + offset, taille - offset, "\nScores : %d - %d\n", jeu->score[0], jeu->score[1]);
}

static int semer(struct Jeu *jeu, int depart)
{
    int graines = jeu->plateau[depart];
    jeu->plateau[depart] = 0;
    int index = depart;
    while (graines > 0) {
        index = (index + 1) % NB_CASES;
        if (index == depart) {
            continue;
        }
        jeu->plateau[index]++;
        graines--;
    }
    return index;
}

static int campDe(int numCase)
{
    return (numCase < 6) ? 0 : 1;
}

static int grainesDansCamp(const struct Jeu *jeu, int joueur)
{
    int total = 0;
    for (int i = 0; i < NB_CASES; i++) {
        if (campDe(i) == joueur) {
            total += jeu->plateau[i];
        }
    }
    return total;
}

static void capturer(struct Jeu *jeu, int caseFinale)
{
    int adversaire = 1 - jeu->joueurCourant;
    int index = caseFinale;
    while (campDe(index) == adversaire && (jeu->plateau[index] == 2 || jeu->plateau[index] == 3)) {
        jeu->score[jeu->joueurCourant] += jeu->plateau[index];
        jeu->plateau[index] = 0;
        if (index == 0) {
            index = NB_CASES - 1;
        } else {
            index--;
        }
    }
}

int coupLegal(const struct Jeu *jeu, int numCaseCliquee)
{
    if (jeu->fini) {
        return 0;
    }
    if (numCaseCliquee < 0 || numCaseCliquee >= NB_CASES) {
        return 0;
    }
    if (campDe(numCaseCliquee) != jeu->joueurCourant) {
        return 0;
    }
    if (jeu->plateau[numCaseCliquee] == 0) {
        return 0;
    }
    int adversaire = 1 - jeu->joueurCourant;
    struct Jeu jeuCopie = *jeu;
    int caseFinale = semer(&jeuCopie, numCaseCliquee);
    capturer(&jeuCopie, caseFinale);
    if (grainesDansCamp(&jeuCopie, adversaire) == 0)
    {
        return 0;
    }
    return 1;
}

static int aUnCoupLegal(const struct Jeu *jeu)
{
    for (int i = 0; i < NB_CASES; i++) {
        if (coupLegal(jeu, i)) {
            return 1;
        }
    }
    return 0;
}

int jouerCoup(struct Jeu *jeu, int numCaseCliquee)
{
    if (!coupLegal(jeu, numCaseCliquee)) {
        return -1;
    }
    int caseFinale = semer(jeu, numCaseCliquee);
    capturer(jeu, caseFinale);
    jeu->nbCoups++;
    jeu->joueurCourant = 1 - jeu->joueurCourant;
    if (jeu->score[0] > 24 || jeu->score[1] > 24 || jeu->nbCoups >= NB_COUPS_MAX)
    {
        jeu->fini = 1;
    }
    else if (!aUnCoupLegal(jeu))
    {
        jeu->fini = 1;
    }
    return 0;
}

int gagnant(const struct Jeu *jeu)
{
    if (jeu->score[0] > jeu->score[1]) {
        return 0;
    } else if (jeu->score[1] > jeu->score[0]) {
        return 1;
    } else {
        return -1; 
    }
}