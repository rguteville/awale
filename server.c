#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "server.h"
#include "envoi.h"
#include "commandes.h"   /* traiterLigne */
#include "joueurs.h"     /* annoncer */
#include "parties.h"     /* annulerDefi, suspendrePartie, verifierDelais */
#include "observation.h" /* retirerObservateur */

/* Ce fichier ne contient que le réseau : ouvrir le port, accepter les clients,
   découper ce qu'ils envoient en lignes et gérer leur départ.
   Le sens de chaque ligne est traité dans commandes.c */

static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if (err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#else
   signal(SIGPIPE, SIG_IGN); /* Mac/Linux : écrire à un client parti ne doit pas tuer le serveur */
#endif
   srand(time(NULL)); /* sinon rand() donne le même tirage au sort à chaque lancement */
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

static SOCKET init_connection(int port)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = {0};

   if (sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(port);
   sin.sin_family = AF_INET;

   /* relancer le serveur tout de suite après l'avoir arrêté, sans attendre que le port se libère */
   int oui = 1;
   setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&oui, sizeof oui);

   if (bind(sock, (SOCKADDR *)&sin, sizeof sin) == SOCKET_ERROR)
   {
      fprintf(stderr, "Impossible d'ouvrir le port %d : %s\n", port, strerror(errno));
      fprintf(stderr, "Un autre serveur tourne peut-etre deja. Essayez : ./server <autre port>\n");
      exit(EXIT_FAILURE);
   }

   if (listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

static void accepterClient(Serveur *s, SOCKET ecoute, int *max)
{
   SOCKADDR_IN csin = {0};
   socklen_t sinsize = sizeof csin;
   SOCKET csock = accept(ecoute, (SOCKADDR *)&csin, &sinsize);
   if (csock == SOCKET_ERROR)
   {
      perror("accept()");
      return;
   }
   if (s->nbClients == MAX_CLIENTS)
   {
      envoyer(csock, "ERR FULL serveur plein, reessayez plus tard");
      closesocket(csock);
      return;
   }
   if (csock > *max)
   {
      *max = csock;
   }

   Client client = {0};
   client.sock = csock;
   client.etat = NON_IDENTIFIE;
   client.adversaire = INVALID_SOCKET;
   client.partie = -1;
   s->clients[s->nbClients] = client;
   s->nbClients++;
}

/* le client i part (connexion coupée ou QUIT) : on défait tous ses liens, puis on le retire */
static void deconnecterClient(Serveur *s, int i)
{
   /* on garde le pseudo AVANT de retirer le client, qui décale le tableau */
   char nom[PSEUDO_MAX + 1];
   int etaitIdentifie = s->clients[i].etat != NON_IDENTIFIE;
   strcpy(nom, s->clients[i].name);

   switch (s->clients[i].etat)
   {
   case DEFI_ENVOYE:
   case DEFI_RECU:
      annulerDefi(s, i);
      break;
   case EN_PARTIE:
      suspendrePartie(s, i); /* pas de défaite immédiate : la partie attend DELAI_RECONNEXION secondes */
      break;
   case OBSERVATEUR:
      retirerObservateur(s, i);
      break;
   default:
      break;
   }

   closesocket(s->clients[i].sock);
   memmove(s->clients + i, s->clients + i + 1, (s->nbClients - i - 1) * sizeof(Client));
   s->nbClients--;

   /* personne ne connaissait un client non identifié : on ne prévient pas */
   if (etaitIdentifie)
   {
      journal("%s se deconnecte", nom);
      annoncer(s, -1, "%s s'est deconnecte", nom);
   }
}

/* lit ce que le client i a envoyé et traite chaque ligne complète.
   Renvoie 1 si le client doit être déconnecté (connexion fermée ou QUIT) */
static int lireClient(Serveur *s, int i)
{
   Client *client = &s->clients[i];
   int n = recv(client->sock, client->buf + client->bufLen, BUF_SIZE - client->bufLen - 1, 0);
   if (n <= 0)
   {
      return 1; /* 0 : le client a fermé ; < 0 : connexion coupée. Dans les deux cas il est parti */
   }
   client->bufLen += n;

   /* TCP ne garde pas les frontières des messages : on ne traite que les lignes terminées par '\n' */
   char *fin;
   while (!client->quitte && (fin = memchr(client->buf, '\n', client->bufLen)) != NULL)
   {
      *fin = '\0';
      if (fin > client->buf && *(fin - 1) == '\r')
      {
         *(fin - 1) = '\0'; /* telnet envoie "\r\n" */
      }
      traiterLigne(s, i, client->buf);
      int longueur = (fin - client->buf) + 1;
      memmove(client->buf, client->buf + longueur, client->bufLen - longueur);
      client->bufLen -= longueur;
   }

   if (client->bufLen == BUF_SIZE - 1)
   {
      envoyer(client->sock, "ERR SYNTAX ligne trop longue");
      client->bufLen = 0;
   }
   return client->quitte;
}

static void app(int port)
{
   SOCKET sock = init_connection(port);
   int max = sock;
   /* tout l'état du serveur. static : il est mis à zéro au démarrage
      (donc toutes les parties ont active = 0) et ne prend pas de place sur la pile */
   static Serveur serveur;
   Serveur *s = &serveur;
   s->nbClients = 0;
   s->prochainId = lireDernierId() + 1;

   journal("Serveur Awale pret sur le port %d (prochaine partie : %d). Appuyez sur Entree pour l'arreter.",
           port, s->prochainId);

   fd_set rdfs;

   while (1)
   {
      FD_ZERO(&rdfs);
      FD_SET(STDIN_FILENO, &rdfs); /* clavier : Entrée arrête le serveur */
      FD_SET(sock, &rdfs);         /* socket d'écoute : un nouveau client */
      for (int i = 0; i < s->nbClients; i++)
      {
         FD_SET(s->clients[i].sock, &rdfs);
      }

      /* select attend au plus 1 seconde : même sans aucun message, le serveur se réveille
         régulièrement pour vérifier les délais de reconnexion.
         select MODIFIE la struct timeval : il faut la remettre à 1 s à chaque tour */
      struct timeval attente;
      attente.tv_sec = 1;
      attente.tv_usec = 0;

      int nbPrets = select(max + 1, &rdfs, NULL, NULL, &attente);
      if (nbPrets == -1)
      {
         perror("select()");
         exit(errno);
      }

      verifierDelais(s);

      if (nbPrets == 0)
      {
         continue; /* délai écoulé, aucune socket prête */
      }

      if (FD_ISSET(STDIN_FILENO, &rdfs))
      {
         break;
      }
      else if (FD_ISSET(sock, &rdfs))
      {
         accepterClient(s, sock, &max);
      }
      else
      {
         for (int i = 0; i < s->nbClients; i++)
         {
            if (FD_ISSET(s->clients[i].sock, &rdfs))
            {
               if (lireClient(s, i))
               {
                  deconnecterClient(s, i);
               }
               break; /* le tableau a pu bouger : on repasse par select */
            }
         }
      }
   }

   for (int i = 0; i < s->nbClients; i++)
   {
      closesocket(s->clients[i].sock);
   }
   closesocket(sock);
   journal("Serveur arrete.");
}

int main(int argc, char **argv)
{
   int port = PORT_DEFAUT;
   if (argc > 1)
   {
      port = atoi(argv[1]);
      if (port <= 0 || port > 65535)
      {
         fprintf(stderr, "Usage : %s [port]   (port par defaut : %d)\n", argv[0], PORT_DEFAUT);
         return EXIT_FAILURE;
      }
   }

   init();

   app(port);

   end();

   return EXIT_SUCCESS;
}
