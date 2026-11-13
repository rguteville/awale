#ifndef PLATEFORME_H
#define PLATEFORME_H

/* Tout ce qui change entre Windows et Mac/Linux pour les sockets, au même endroit.
   Inclus par le client ET par le serveur. */

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

#endif /* PLATEFORME_H */
