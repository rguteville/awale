#ifndef CLIENT_H
#define CLIENT_H

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
#include "awale.h"
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

#define CRLF     "\r\n"
#define PORT     1977

#define BUF_SIZE 1024

typedef struct
{
   char monPseudo[21];
   char adversaire[21];
   int monCamp;
   int partieObservee;   /* id de la partie observée (WATCH), 0 si on n'observe rien */
} EtatClient;

static void init(void);
static void end(void);
static void app(const char *address, const char *name);
static int init_connection(const char *address);
static void end_connection(int sock);
static void write_server(SOCKET sock, const char *buffer);
static void traiterMessage(EtatClient *e, char *ligne);

#endif /* guard */
