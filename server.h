#ifndef SERVER_H
#define SERVER_H

/* Les types partagés par tous les fichiers du serveur.
   Chaque module (joueurs.c, parties.c...) a en plus son propre .h avec ses fonctions. */

#include <time.h> /* time_t */

#include "plateforme.h" /* SOCKET, INVALID_SOCKET... */
#include "awale.h"
#include "commun.h"     /* PSEUDO_MAX, BIO_LIGNES_MAX, BIO_LIGNE_MAX, BUF_SIZE */
#include "sauvegarde.h" /* PartieSauvee */

#define MAX_CLIENTS 100
#define MAX_PARTIES 50
#define MAX_OBSERVATEURS 10
#define MAX_AMIS 20
#define HISTORIQUE_MAX 10

/* secondes laissées à un joueur déconnecté pour revenir dans sa partie.
   #ifndef : on peut le changer sans toucher au code, ex. pour tester : make CFLAGS="-DDELAI_RECONNEXION=3" */
#ifndef DELAI_RECONNEXION
#define DELAI_RECONNEXION 30
#endif

enum Etat
{
   NON_IDENTIFIE,
   LIBRE,
   DEFI_ENVOYE,
   DEFI_RECU,
   EN_PARTIE,
   OBSERVATEUR
};

typedef struct
{
   SOCKET sock;
   char name[PSEUDO_MAX + 1]; /* +1 pour le '\0' final */
   enum Etat etat;
   char buf[BUF_SIZE]; /* octets reçus mais pas encore traités */
   int bufLen;
   SOCKET adversaire;
   int partie; /* indice de la partie jouée ou observée, -1 sinon */
   char bio[BIO_LIGNES_MAX][BIO_LIGNE_MAX + 1];
   int nbLignesBio;
   char amis[MAX_AMIS][PSEUDO_MAX + 1];
   int nbAmis;
   int prive;
   int quitte; /* 1 après QUIT : le serveur ferme la connexion après avoir traité la ligne */
} Client;

typedef struct
{
   int active;                            /* 1 si la partie est en cours, 0 si la case est libre */
   int id;                                /* identifiant affiché aux joueurs (START, BOARD, WATCH <id>) */
   struct Jeu jeu;                        /* le moteur */
   SOCKET joueurs[2];                     /* joueurs[0] joue le camp 0, joueurs[1] le camp 1.
                                             INVALID_SOCKET si le joueur est déconnecté (partie en pause) */
   time_t deconnecteDepuis[2];            /* heure de la déconnexion du joueur de ce camp, 0 s'il est là */
   SOCKET observateurs[MAX_OBSERVATEURS];
   int nbObservateurs;
   PartieSauvee historique; /* pseudos, coups joués... : ce qui sera écrit dans parties.txt */
} Partie;

/* tout l'état du serveur : chaque fonction reçoit un Serveur * au lieu de plusieurs paramètres.
   Ajouter une fonctionnalité = ajouter un champ ici, sans changer les signatures */
typedef struct
{
   Client clients[MAX_CLIENTS];
   int nbClients;
   Partie parties[MAX_PARTIES];
   int prochainId; /* id de la prochaine partie créée */
} Serveur;

#endif /* SERVER_H */
