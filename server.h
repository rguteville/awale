#ifndef SERVER_H
#define SERVER_H

#ifdef WIN32

#include <winsock2.h>
typedef int socklen_t;

#elif defined (__linux__) || defined (__APPLE__)

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h> /* select, fd_set */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h> /* gethostbyname */

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

#include "awale.h"

#define CRLF        "\r\n"
#define PORT         1977
#define MAX_CLIENTS     100

#define BUF_SIZE    1024
#define PSEUDO_MAX 20

#define MAX_PARTIES      50
#define MAX_OBSERVATEURS 10

enum Etat { NON_IDENTIFIE, LIBRE, DEFI_ENVOYE, DEFI_RECU, EN_PARTIE, OBSERVATEUR };
typedef struct
{
   SOCKET sock;
   char name[PSEUDO_MAX + 1];  /* +1 pour le '\0' final */
   enum Etat etat;
   char buf[BUF_SIZE];         /* octets reçus mais pas encore traités */
   int bufLen;   
   SOCKET adversaire;
   int partie;                  /* indice de la partie en cours, ou -1 si pas en partie */              
} Client;

typedef struct
{
   int active;                              /* 1 si la partie est en cours, 0 si la case est libre */
   int id;                                  /* identifiant affiché aux joueurs (START, BOARD, WATCH <id>) */
   struct Jeu jeu;                          /* votre moteur */
   SOCKET joueurs[2];                       /* joueurs[0] joue le camp 0, joueurs[1] le camp 1 */
   SOCKET observateurs[MAX_OBSERVATEURS];   /* pour plus tard */
   int nbObservateurs;
} Partie;

/* tout l'état du serveur : chaque fonction reçoit un Serveur * au lieu de plusieurs paramètres.
   Ajouter une fonctionnalité = ajouter un champ ici, sans changer les signatures */
typedef struct
{
   Client clients[MAX_CLIENTS];
   int nbClients;                  /* nombre de clients connectés (l'ancien "actual") */
   Partie parties[MAX_PARTIES];
   int prochainId;                 /* id de la prochaine partie créée, commence à 1 */
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
static int trouverParPseudo(Serveur *s, const char *pseudo);  /* indice, ou -1 */
static int trouverParSocket(Serveur *s, SOCKET sock);         /* indice, ou -1 */
static void cmdChallenge(Serveur *s, int i, char *args);
static void cmdRefuse(Serveur *s, int i);
static void cmdAccept(Serveur *s, int i);
static void envoyerPlateau(Serveur *s, Partie *partie);
static void cmdPlay(Serveur *s, int i, char *args);
static void terminerPartie(Serveur *s, Partie *partie, const char *gagnant, const char *raison);
static void cmdForfeit(Serveur *s, int i);
static void abandonnerPartie(Serveur *s, int i, const char *raison);  /* i perd, son adversaire gagne */
static void cmdCancel(Serveur *s, int i);

#endif /* guard */
