#include <stdio.h>
#include <stdlib.h>

/* on inclut le .c (et pas le .h) pour pouvoir tester les fonctions static
   comme semer et capturer : elles font alors partie de ce fichier.
   => compiler avec : cc -Wall -o test_awale test_awale.c   (SANS awale.c) */
#include "awale.c"

static int nbEchecs = 0;

/* affiche OK ou ECHEC selon la condition */
static void verifier(int condition, const char *description)
{
    if (condition) {
        printf("  OK     %s\n", description);
    } else {
        printf("  ECHEC  %s\n", description);
        nbEchecs++;
    }
}

/* met le jeu dans un état choisi : valeurs[i] = graines de la case i */
static void remplir(struct Jeu *jeu, const int valeurs[NB_CASES], int joueur)
{
    initJeu(jeu, joueur);
    for (int i = 0; i < NB_CASES; i++) {
        jeu->plateau[i] = valeurs[i];
    }
}

static void afficher(const struct Jeu *jeu)
{
    char texte[512];
    plateauEnTexte(jeu, texte, sizeof texte);
    printf("%s", texte);
}

static void testInit(void)
{
    struct Jeu jeu;
    int toutes4 = 1;

    printf("\n[initJeu]\n");
    initJeu(&jeu, 1);
    for (int i = 0; i < NB_CASES; i++) {
        if (jeu.plateau[i] != GRAINES_INIT) {
            toutes4 = 0;
        }
    }
    verifier(toutes4, "4 graines dans chaque case");
    verifier(jeu.score[0] == 0 && jeu.score[1] == 0, "scores a 0");
    verifier(jeu.joueurCourant == 1, "le premier joueur est bien 1");
    verifier(jeu.fini == 0, "partie pas finie");
}

static void testSemerSimple(void)
{
    struct Jeu jeu;
    /* plateau de l'exercice (cases 0 a 11) */
    int p[NB_CASES] = {4, 4, 4, 4, 4, 2,   1, 2, 1, 4, 1, 1};

    printf("\n[semer] cas simple : joueur 0 joue la case 5 (2 graines)\n");
    remplir(&jeu, p, 0);
    int derniere = semer(&jeu, 5);
    verifier(derniere == 7, "renvoie 7");
    verifier(jeu.plateau[5] == 0, "case 5 videe");
    verifier(jeu.plateau[6] == 2, "case 6 : 1 -> 2");
    verifier(jeu.plateau[7] == 3, "case 7 : 2 -> 3");
    verifier(jeu.plateau[8] == 1, "case 8 inchangee");
}

static void testSemerSaut(void)
{
    struct Jeu jeu;
    int p[NB_CASES] = {13, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0};

    printf("\n[semer] 13 graines : la case de depart doit etre sautee\n");
    remplir(&jeu, p, 0);
    int derniere = semer(&jeu, 0);
    /* 11 graines dans les cases 1 a 11, on saute 0, puis 2 graines en 1 et 2 */
    verifier(derniere == 2, "renvoie 2");
    verifier(jeu.plateau[0] == 0, "case 0 reste vide (sautee)");
    verifier(jeu.plateau[1] == 2 && jeu.plateau[2] == 2, "cases 1 et 2 : 2 graines");
    verifier(jeu.plateau[3] == 1 && jeu.plateau[11] == 1, "cases 3 a 11 : 1 graine");
}

static void testCaptureChaineJoueur0(void)
{
    struct Jeu jeu;
    int p[NB_CASES] = {4, 4, 4, 4, 4, 2,   1, 2, 1, 4, 1, 1};

    printf("\n[capturer] joueur 0, chaine : cases 7 (3) puis 6 (2)\n");
    remplir(&jeu, p, 0);
    capturer(&jeu, semer(&jeu, 5));
    verifier(jeu.score[0] == 5, "score joueur 0 = 5");
    verifier(jeu.plateau[6] == 0 && jeu.plateau[7] == 0, "cases 6 et 7 videes");
    verifier(jeu.plateau[8] == 1, "case 8 non touchee");
    afficher(&jeu);
}

static void testPasDeCaptureDansSonCamp(void)
{
    struct Jeu jeu;

    printf("\n[capturer] joueur 0 finit dans son propre camp : rien\n");
    initJeu(&jeu, 0);
    capturer(&jeu, semer(&jeu, 0));   /* 4 graines : cases 1 a 4 */
    verifier(jeu.score[0] == 0, "score joueur 0 = 0");
}

static void testCaptureChaineJoueur1(void)
{
    struct Jeu jeu;
    /* case 11 : 2 graines -> vont en 0 et 1 ; 0 : 1->2, 1 : 2->3 */
    int p[NB_CASES] = {1, 2, 4, 4, 4, 4,   4, 4, 4, 4, 4, 2};

    printf("\n[capturer] joueur 1, chaine : cases 1 (3) puis 0 (2), puis stop en 11\n");
    remplir(&jeu, p, 1);
    capturer(&jeu, semer(&jeu, 11));
    verifier(jeu.score[1] == 5, "score joueur 1 = 5");
    verifier(jeu.plateau[0] == 0 && jeu.plateau[1] == 0, "cases 0 et 1 videes");
    afficher(&jeu);
}

static void testJoueur1PasDeCapture(void)
{
    struct Jeu jeu;
    /* case 11 : 1 graine -> va en 0, qui passe de 4 a 5 : pas 2 ou 3 */
    int p[NB_CASES] = {4, 4, 4, 4, 4, 4,   4, 4, 4, 4, 4, 1};

    printf("\n[capturer] joueur 1 tombe sur une case a 5 graines : rien\n");
    remplir(&jeu, p, 1);
    capturer(&jeu, semer(&jeu, 11));
    verifier(jeu.score[1] == 0, "score joueur 1 = 0");
    verifier(jeu.plateau[0] == 5, "case 0 garde ses 5 graines");
    afficher(&jeu);
}

static void testJoueur1ChaineCassee(void)
{
    struct Jeu jeu;
    /* case 11 : 2 graines -> 0 : 4->5, 1 : 1->2 ; on capture 1, mais pas 0 */
    int p[NB_CASES] = {4, 1, 4, 4, 4, 4,   4, 4, 4, 4, 4, 2};

    printf("\n[capturer] joueur 1 : capture case 1 (2), la chaine s'arrete en 0 (5)\n");
    remplir(&jeu, p, 1);
    capturer(&jeu, semer(&jeu, 11));
    verifier(jeu.score[1] == 2, "score joueur 1 = 2");
    verifier(jeu.plateau[0] == 5, "case 0 garde ses 5 graines");
    afficher(&jeu);
}

static void testGrainesDansCamp(void)
{
    struct Jeu jeu;
    int p[NB_CASES] = {4, 4, 4, 4, 4, 2,   1, 2, 1, 4, 1, 1};

    printf("\n[grainesDansCamp]\n");
    remplir(&jeu, p, 0);
    verifier(grainesDansCamp(&jeu, 0) == 22, "joueur 0 : 22 graines");
    verifier(grainesDansCamp(&jeu, 1) == 10, "joueur 1 : 10 graines");
}

/* 1 si les deux jeux sont identiques (plateau, scores, joueur courant) */
static int memeJeu(const struct Jeu *a, const struct Jeu *b)
{
    for (int i = 0; i < NB_CASES; i++) {
        if (a->plateau[i] != b->plateau[i]) {
            return 0;
        }
    }
    return a->score[0] == b->score[0] && a->score[1] == b->score[1]
        && a->joueurCourant == b->joueurCourant;
}

static void testCoupLegalBases(void)
{
    struct Jeu jeu;
    int p[NB_CASES] = {4, 0, 4, 4, 4, 4,   4, 4, 4, 4, 4, 4};

    printf("\n[coupLegal] cas de base, joueur 0\n");
    remplir(&jeu, p, 0);
    verifier(coupLegal(&jeu, 0) == 1, "case 0 (4 graines, son camp) : legal");
    verifier(coupLegal(&jeu, -1) == 0, "case -1 : illegal");
    verifier(coupLegal(&jeu, 12) == 0, "case 12 : illegal");
    verifier(coupLegal(&jeu, 999) == 0, "case 999 : illegal");
    verifier(coupLegal(&jeu, 7) == 0, "case 7 (camp adverse) : illegal");
    verifier(coupLegal(&jeu, 1) == 0, "case 1 (vide) : illegal");

    printf("\n[coupLegal] cas de base, joueur 1\n");
    remplir(&jeu, p, 1);
    verifier(coupLegal(&jeu, 6) == 1, "case 6 (son camp) : legal");
    verifier(coupLegal(&jeu, 0) == 0, "case 0 (camp adverse) : illegal");
}

static void testCoupLegalNourrir(void)
{
    struct Jeu jeu;
    /* le joueur 1 n'a plus rien ; case 5 -> sa graine va en 6 (nourrit),
       case 2 -> sa graine va en 3 (ne nourrit pas) */
    int p[NB_CASES] = {0, 0, 1, 0, 0, 1,   0, 0, 0, 0, 0, 0};

    printf("\n[coupLegal] obligation de nourrir l'adversaire\n");
    remplir(&jeu, p, 0);
    verifier(coupLegal(&jeu, 5) == 1, "case 5 nourrit l'adversaire : legal");
    verifier(coupLegal(&jeu, 2) == 0, "case 2 ne nourrit pas : illegal");
}

static void testCoupLegalImpossibleDeNourrir(void)
{
    struct Jeu jeu;
    /* le joueur 1 n'a plus rien et le joueur 0 ne peut pas l'atteindre */
    int p[NB_CASES] = {1, 1, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0};
    int auMoinsUnLegal = 0;

    printf("\n[coupLegal] personne ne peut nourrir : aucun coup legal\n");
    remplir(&jeu, p, 0);
    for (int i = 0; i < NB_CASES; i++) {
        if (coupLegal(&jeu, i)) {
            auMoinsUnLegal = 1;
        }
    }
    verifier(auMoinsUnLegal == 0, "aucune case jouable");
}

static void testJouerCoupIllegal(void)
{
    struct Jeu jeu;
    struct Jeu avant;

    printf("\n[jouerCoup] coup illegal : rien ne doit changer\n");
    initJeu(&jeu, 0);
    avant = jeu;
    verifier(jouerCoup(&jeu, 8) == -1, "case 8 (camp adverse) renvoie -1");
    verifier(memeJeu(&jeu, &avant), "le jeu n'a pas bouge");
    verifier(jouerCoup(&jeu, 42) == -1, "case 42 renvoie -1");
    verifier(memeJeu(&jeu, &avant), "le jeu n'a toujours pas bouge");
}

static void testJouerCoupLegal(void)
{
    struct Jeu jeu;

    printf("\n[jouerCoup] coup legal depuis le debut : joueur 0 joue la case 0\n");
    initJeu(&jeu, 0);
    verifier(jouerCoup(&jeu, 0) == 0, "renvoie 0");
    verifier(jeu.plateau[0] == 0, "case 0 videe");
    verifier(jeu.plateau[1] == 5 && jeu.plateau[4] == 5, "cases 1 a 4 : 5 graines");
    verifier(jeu.plateau[5] == 4, "case 5 inchangee");
    verifier(jeu.joueurCourant == 1, "c'est au tour du joueur 1");
    verifier(jouerCoup(&jeu, 1) == -1, "le joueur 0 ne peut pas rejouer tout de suite");
}

static void testJouerCoupAvecCapture(void)
{
    struct Jeu jeu;
    int p[NB_CASES] = {4, 4, 4, 4, 4, 2,   1, 2, 1, 4, 1, 1};

    printf("\n[jouerCoup] semer + capturer + changer de joueur\n");
    remplir(&jeu, p, 0);
    verifier(jouerCoup(&jeu, 5) == 0, "renvoie 0");
    verifier(jeu.score[0] == 5, "score joueur 0 = 5");
    verifier(jeu.joueurCourant == 1, "c'est au tour du joueur 1");
    afficher(&jeu);
}

static void testGrandChelem(void)
{
    struct Jeu jeu;
    /* case 5 : 2 graines -> 6 : 1->2, 7 : 2->3 ; capturer 7 et 6 prendrait TOUT */
    int p[NB_CASES] = {4, 4, 4, 4, 4, 2,   1, 2, 0, 0, 0, 0};
    /* meme chose, mais il reste 1 graine en 8 apres la capture */
    int q[NB_CASES] = {4, 4, 4, 4, 4, 2,   1, 2, 1, 0, 0, 0};

    printf("\n[coupLegal] grand chelem interdit\n");
    remplir(&jeu, p, 0);
    verifier(coupLegal(&jeu, 5) == 0, "case 5 prendrait tout : illegal");
    verifier(jouerCoup(&jeu, 5) == -1, "jouerCoup refuse aussi (-1)");

    remplir(&jeu, q, 0);
    verifier(coupLegal(&jeu, 5) == 1, "avec 1 graine en 8, case 5 : legal");
    verifier(jouerCoup(&jeu, 5) == 0 && jeu.score[0] == 5, "capture 5, il reste 1 graine en 8");
}

static void testFinParScore(void)
{
    struct Jeu jeu;
    int p[NB_CASES] = {4, 4, 4, 4, 4, 2,   1, 2, 1, 4, 1, 1};

    printf("\n[fin de partie] un joueur depasse 24\n");
    remplir(&jeu, p, 0);
    jeu.score[0] = 20;
    jouerCoup(&jeu, 5);               /* capture 5 -> 25 */
    verifier(jeu.score[0] == 25, "score joueur 0 = 25");
    verifier(jeu.fini == 1, "partie finie");
    verifier(gagnant(&jeu) == 0, "gagnant : joueur 0");

    /* le joueur 1 a un coup qui serait legal (case 9, 4 graines), mais la partie est finie */
    verifier(jouerCoup(&jeu, 9) == -1, "plus aucun coup accepte apres la fin");
}

static void testFinPlusDeCoupLegal(void)
{
    struct Jeu jeu;
    /* joueur 0 joue sa derniere graine (5 -> 6, qui passe a 4 : pas de capture).
       Le joueur 1 doit alors nourrir le joueur 0, mais 4 graines depuis 6
       vont en 7, 8, 9, 10 : impossible -> fin */
    int p[NB_CASES] = {0, 0, 0, 0, 0, 1,   3, 0, 0, 0, 0, 0};

    printf("\n[fin de partie] le joueur suivant ne peut pas nourrir\n");
    remplir(&jeu, p, 0);
    verifier(jouerCoup(&jeu, 5) == 0, "le coup du joueur 0 est accepte");
    verifier(jeu.fini == 1, "partie finie");
    verifier(gagnant(&jeu) == -1, "0 - 0 : egalite (graines restantes a personne)");
}

static void testPasFiniAuDebut(void)
{
    struct Jeu jeu;

    printf("\n[fin de partie] un coup normal ne termine pas la partie\n");
    initJeu(&jeu, 0);
    jouerCoup(&jeu, 0);
    verifier(jeu.fini == 0, "partie pas finie");
}

static void testGagnant(void)
{
    struct Jeu jeu;

    printf("\n[gagnant]\n");
    initJeu(&jeu, 0);
    jeu.score[0] = 10;
    jeu.score[1] = 26;
    verifier(gagnant(&jeu) == 1, "10 - 26 : joueur 1");
    jeu.score[0] = 24;
    jeu.score[1] = 24;
    verifier(gagnant(&jeu) == -1, "24 - 24 : egalite");
}

static void testLimiteCoups(void)
{
    struct Jeu jeu;

    printf("\n[fin de partie] limite de %d coups\n", NB_COUPS_MAX);
    initJeu(&jeu, 0);
    jouerCoup(&jeu, 0);
    verifier(jeu.nbCoups == 1, "un coup legal : nbCoups = 1");
    jouerCoup(&jeu, 0);               /* case 0 du joueur 1 : illegal */
    verifier(jeu.nbCoups == 1, "un coup illegal ne compte pas");

    /* on saute directement a la fin plutot que de jouer 48 coups */
    initJeu(&jeu, 0);
    jeu.nbCoups = NB_COUPS_MAX - 2;
    jouerCoup(&jeu, 0);
    verifier(jeu.fini == 0, "avant-dernier coup : partie pas finie");
    jouerCoup(&jeu, 6);
    verifier(jeu.nbCoups == NB_COUPS_MAX, "nbCoups atteint la limite");
    verifier(jeu.fini == 1, "dernier coup : partie finie");
    verifier(jouerCoup(&jeu, 1) == -1, "plus aucun coup accepte");
}

int main(void)
{
    testInit();
    testSemerSimple();
    testSemerSaut();
    testCaptureChaineJoueur0();
    testPasDeCaptureDansSonCamp();
    testCaptureChaineJoueur1();
    testJoueur1PasDeCapture();
    testJoueur1ChaineCassee();
    testGrainesDansCamp();
    testCoupLegalBases();
    testCoupLegalNourrir();
    testCoupLegalImpossibleDeNourrir();
    testJouerCoupIllegal();
    testJouerCoupLegal();
    testJouerCoupAvecCapture();
    testGrandChelem();
    testFinParScore();
    testFinPlusDeCoupLegal();
    testPasFiniAuDebut();
    testGagnant();
    testLimiteCoups();

    printf("\n%d echec(s)\n", nbEchecs);
    return nbEchecs == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
