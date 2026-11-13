#ifndef CLIENT_H
#define CLIENT_H

/* Les types partagés par tous les fichiers du client :
   client.c (réseau), saisie.c (ce que tape le joueur), messages.c (ce que dit le serveur),
   affichage.c (couleurs, plateau, invite). */

#include "plateforme.h"
#include "awale.h"
#include "commun.h" /* PORT_DEFAUT, BUF_SIZE, PSEUDO_MAX */

/* après une coupure : RECONNEXION_ESSAIS tentatives, espacées de RECONNEXION_PAUSE secondes
   (15 x 2 s = 30 s, le délai pendant lequel le serveur garde la partie en pause) */
#define RECONNEXION_ESSAIS 15
#define RECONNEXION_PAUSE 2

#define MAX_PARTIES_CONNUES 50

/* une partie terminée revue coup par coup (REPLAY) : le serveur envoie toutes les positions d'un coup,
   le client les garde et le joueur avance avec Entrée */
typedef struct
{
   int actif;   /* 1 entre OK REPLAY et la sortie du rejeu */
   int complet; /* 1 quand END est arrivé : on peut naviguer */
   int id;
   char noms[2][PSEUDO_MAX + 1];
   struct Jeu positions[NB_COUPS_MAX + 1]; /* position de départ + une par coup */
   int nb;
   int courant; /* position affichée */
   char gagnant[PSEUDO_MAX + 1];
   char raison[16];
} Rejeu;

/* une partie en cours vue dans la réponse à GAMES : sert à afficher les noms quand on l'observe */
typedef struct
{
   int id;
   char noms[2][PSEUDO_MAX + 1];
} PartieConnue;

typedef struct
{
   SOCKET sock;
   char monPseudo[PSEUDO_MAX + 1]; /* pseudo ACCEPTÉ par le serveur (mis à jour sur OK LOGIN) */
   int identifie;                  /* 0 tant que le serveur n'a pas répondu OK LOGIN */
   int dejaConnecte;               /* 1 après le premier OK LOGIN : les suivants sont des reconnexions */
   int quitte;                     /* 1 après QUIT : la fermeture de la connexion est normale */
   char derniereCommande[16];      /* dernière commande envoyée : sert à expliquer les erreurs */
   char confirmation[16];          /* commande qui attend d'être retapée pour confirmer ("" sinon) */

   /* défis */
   char defiDe[PSEUDO_MAX + 1]; /* qui nous a défié, "" sinon */
   char defiA[PSEUDO_MAX + 1];  /* qui on a défié, "" sinon */

   /* partie jouée */
   int partie; /* id de la partie jouée, 0 sinon */
   int monCamp;
   char adversaire[PSEUDO_MAX + 1];
   int enPause;
   int dernierePartie; /* id de la dernière partie terminée, pour proposer de la revoir */

   /* partie observée */
   int partieObservee; /* id, 0 si on n'observe rien */

   /* plateau affiché : noms des deux camps et position précédente (pour repérer le coup joué) */
   char noms[2][PSEUDO_MAX + 1];
   struct Jeu dernier;
   int aDernier;

   int reglesVues;  /* le rappel des règles n'est affiché qu'à la première partie */
   int bioLignes;   /* lignes de la bio en cours d'affichage */
   int gamesMuets;  /* réponses GAMES à ranger sans les afficher (avant un WATCH) */
   int accueil;     /* réponses USERS / RANKING de l'accueil, à afficher en version courte */
   char msgDest[PSEUDO_MAX + 1]; /* dernier message privé envoyé, affiché quand le serveur confirme */
   char msgTexte[BUF_SIZE];

   PartieConnue connues[MAX_PARTIES_CONNUES];
   int nbConnues;

   Rejeu rejeu;
} EtatClient;

/* envoie une ligne au serveur, comme printf ; le '\n' est ajouté ici */
void envoyerAuServeur(EtatClient *e, const char *format, ...);

#endif /* CLIENT_H */
