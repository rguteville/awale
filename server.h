#ifndef SERVER_H
#define SERVER_H

#ifdef WIN32

#include <winsock2.h>
typedef int socklen_t;

#elif defined(__linux__) || defined(__APPLE__)

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h> /* select, fd_set */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h>  /* gethostbyname */

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

#else

#error not defined for this platform

#endif

#include <time.h>         /* time_t, time, difftime */

#include "awale.h"
#include "commun.h"       /* PSEUDO_MAX, BIO_LIGNES_MAX, BIO_LIGNE_MAX */
#include "sauvegarde.h"   /* PartieSauvee, sauverPartie, ... */

#define CRLF "\r\n"
#define PORT 1977
#define MAX_CLIENTS 100

#define BUF_SIZE 1024
#define HISTORIQUE_MAX 10

#define MAX_PARTIES 50
#define MAX_OBSERVATEURS 10
#define MAX_AMIS 20

#define DELAI_RECONNEXION 30 /* secondes laissées à un joueur déconnecté pour revenir dans sa partie */

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
   int partie; /* indice de la partie en cours, ou -1 si pas en partie */
   char bio[BIO_LIGNES_MAX][BIO_LIGNE_MAX + 1];
   int nbLignesBio;
   char amis[MAX_AMIS][PSEUDO_MAX + 1];
   int nbAmis;   
   int prive;    


} Client;

typedef struct
{
   int active;                            /* 1 si la partie est en cours, 0 si la case est libre */
   int id;                                /* identifiant affiché aux joueurs (START, BOARD, WATCH <id>) */
   struct Jeu jeu;                        /* votre moteur */
   SOCKET joueurs[2];                     /* joueurs[0] joue le camp 0, joueurs[1] le camp 1.
                                             INVALID_SOCKET si le joueur est déconnecté (partie en pause) */
   time_t deconnecteDepuis[2];            /* heure de la déconnexion du joueur de ce camp, 0 s'il est là */
   SOCKET observateurs[MAX_OBSERVATEURS]; /* pour plus tard */
   int nbObservateurs;
   PartieSauvee historique;

} Partie;

/* tout l'état du serveur : chaque fonction reçoit un Serveur * au lieu de plusieurs paramètres.
   Ajouter une fonctionnalité = ajouter un champ ici, sans changer les signatures */
typedef struct
{
   Client clients[MAX_CLIENTS];
   int nbClients; /* nombre de clients connectés (l'ancien "actual") */
   Partie parties[MAX_PARTIES];
   int prochainId; /* id de la prochaine partie créée, commence à 1 */
} Serveur;

static void init(void);
static void end(void);
static void app(void);
static int init_connection(void);
static void end_connection(int sock);
static void write_client(SOCKET sock, const char *buffer);
static void remove_client(Serveur *s, int to_remove);
static void clear_clients(Serveur *s);
static void traiterLigne(Serveur *s, int i, char *ligne);
static void cmdLogin(Serveur *s, int i, char *args);
static void cmdList(Serveur *s, int i);
static void envoyer(SOCKET sock, const char *format, ...);
static int trouverParPseudo(Serveur *s, const char *pseudo); /* indice, ou -1 */
static int trouverParSocket(Serveur *s, SOCKET sock);        /* indice, ou -1 */
static void cmdChallenge(Serveur *s, int i, char *args);
static void cmdRefuse(Serveur *s, int i);
static void cmdAccept(Serveur *s, int i);
static void formaterPlateau(int id, const struct Jeu *jeu, char *ligne, size_t taille); /* écrit "BOARD ..." dans ligne, n'envoie rien */
static void envoyerPlateau(Serveur *s, Partie *partie);
static void cmdPlay(Serveur *s, int i, char *args);
static void terminerPartie(Serveur *s, Partie *partie, const char *gagnant, const char *raison);
static void cmdForfeit(Serveur *s, int i);
static void abandonnerPartie(Serveur *s, int i, const char *raison); /* i perd, son adversaire gagne */
static void cmdCancel(Serveur *s, int i);
static void cmdGames(Serveur *s, int i);
static void cmdWatch(Serveur *s, int i, char *args);
static void cmdUnwatch(Serveur *s, int i);
static int trouverPartieParId(Serveur *s, int idPartie);
static void retirerObservateur(Serveur *s, int i);
static void cmdBio(Serveur *s, int i, char *args);
static void cmdBioClear(Serveur *s, int i);
static void cmdBioAdd(Serveur *s, int i, char *args);
static void cmdMsg(Serveur *s, int i, char *args);
static void cmdAll(Serveur *s, int i, char *args);
static void cmdReplay(Serveur *s, int i, char *args);
static void cmdFriend(Serveur *s, int i, char *args);
static void cmdUnfriend(Serveur *s, int i, char *args);
static void cmdPrivate(Serveur *s, int i, char *args);
static int estAmi(Serveur *s, int i, const char *pseudo);
static void cmdFriends(Serveur *s, int i); /* répond "FRIENDS <p1> <p2> ..." */
static void cmdHistory(Serveur *s, int i, char *args);
static int campDuClient(const Partie *partie, SOCKET sock);           /* 0 ou 1 */
static void prevenirPartie(const Partie *partie, const char *ligne);  /* aux joueurs présents + observateurs */
static void suspendrePartie(Serveur *s, int i);                       /* i se déconnecte en partie */
static void reprendrePartie(Serveur *s, int i);                       /* après LOGIN : i avait une partie en pause ? */
static void verifierDelais(Serveur *s);                               /* fin des parties dont le délai est dépassé */



#endif /* guard */
