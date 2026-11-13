#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>

#include "client.h"
#include "affichage.h"
#include "messages.h" /* traiterMessage */
#include "saisie.h"   /* traiterSaisie */

/* Ce fichier ne contient que le réseau : se connecter, se reconnecter après une coupure,
   attendre (select) le clavier ou le serveur, et découper ce qui arrive en lignes.
   Le sens des lignes est traité dans saisie.c (clavier) et messages.c (serveur). */

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
   /* Mac/Linux : écrire sur une connexion coupée ne doit pas tuer le client,
      sinon il n'aurait pas le temps de se reconnecter */
   signal(SIGPIPE, SIG_IGN);
#endif
   initAffichage();
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

void envoyerAuServeur(EtatClient *e, const char *format, ...)
{
   char ligne[BUF_SIZE];
   va_list args;
   va_start(args, format);
   vsnprintf(ligne, sizeof ligne - 1, format, args); /* -1 : on garde la place du '\n' */
   va_end(args);
   strcat(ligne, "\n");
   /* pas d'exit si l'envoi échoue : la coupure sera vue par recv() dans app(), qui se reconnectera */
   send(e->sock, ligne, strlen(ligne), 0);
}

/* INVALID_SOCKET si la connexion échoue */
static SOCKET tenterConnexion(const char *adresse, int port)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = {0};
   if (sock == INVALID_SOCKET)
   {
      return INVALID_SOCKET;
   }
   struct hostent *hostinfo = gethostbyname(adresse);
   if (hostinfo == NULL)
   {
      closesocket(sock);
      return INVALID_SOCKET;
   }
   sin.sin_addr = *(IN_ADDR *)hostinfo->h_addr_list[0];
   sin.sin_port = htons(port);
   sin.sin_family = AF_INET;
   if (connect(sock, (SOCKADDR *)&sin, sizeof(SOCKADDR)) == SOCKET_ERROR)
   {
      closesocket(sock);
      return INVALID_SOCKET;
   }
   return sock;
}

/* après une coupure : plusieurs essais, puis LOGIN avec le même pseudo.
   Le serveur nous remet dans notre partie s'il l'a gardée en pause (RESUME) */
static int reconnecter(EtatClient *e, const char *adresse, int port)
{
   printf("\n");
   alerte("Connexion au serveur perdue. Reconnexion en cours (%d s max)…", RECONNEXION_ESSAIS * RECONNEXION_PAUSE);
   for (int essai = 1; essai <= RECONNEXION_ESSAIS; essai++)
   {
#ifdef WIN32
      Sleep(RECONNEXION_PAUSE * 1000);
#else
      sleep(RECONNEXION_PAUSE);
#endif
      SOCKET sock = tenterConnexion(adresse, port);
      if (sock != INVALID_SOCKET)
      {
         e->sock = sock;
         /* on repart de zéro : c'est le serveur qui dira, avec RESUME, si une partie reprend */
         e->identifie = 0;
         e->partie = 0;
         e->monCamp = -1;
         e->enPause = 0;
         e->partieObservee = 0;
         e->aDernier = 0;
         e->defiA[0] = '\0';
         e->defiDe[0] = '\0';
         e->rejeu.actif = 0;
         envoyerAuServeur(e, "LOGIN %s", e->monPseudo);
         return 1;
      }
   }
   return 0;
}

static void app(const char *adresse, int port, const char *pseudo)
{
   /* static : l'état (avec les positions du rejeu) est un peu gros pour la pile, et il est mis à zéro */
   static EtatClient etat;
   EtatClient *e = &etat;
   e->monCamp = -1;

   e->sock = tenterConnexion(adresse, port);
   if (e->sock == INVALID_SOCKET)
   {
      echec("Impossible de joindre le serveur %s sur le port %d. Est-il bien lancé ?", adresse, port);
      exit(EXIT_FAILURE);
   }
   afficherBanniere(adresse, port);

   if (pseudo != NULL)
   {
      snprintf(e->derniereCommande, sizeof e->derniereCommande, "LOGIN");
      envoyerAuServeur(e, "LOGIN %s", pseudo);
   }
   else
   {
      texte("Choisissez un pseudo (1 à 20 lettres, chiffres ou _) :");
      afficherInvite(e);
   }

   char recu[BUF_SIZE]; /* octets reçus du serveur mais pas encore traités */
   int recuLen = 0;
   fd_set rdfs;

   while (1)
   {
      FD_ZERO(&rdfs);
      FD_SET(STDIN_FILENO, &rdfs);
      FD_SET(e->sock, &rdfs);

      if (select(e->sock + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      if (FD_ISSET(STDIN_FILENO, &rdfs))
      {
         char saisie[BUF_SIZE];
         if (fgets(saisie, sizeof saisie - 1, stdin) == NULL)
         {
            /* Ctrl-D ou fin du fichier d'entrée : on part proprement */
            e->quitte = 1;
            envoyerAuServeur(e, "QUIT");
            printf("\n");
            break;
         }
         traiterSaisie(e, saisie);
      }
      else if (FD_ISSET(e->sock, &rdfs))
      {
         int n = recv(e->sock, recu + recuLen, BUF_SIZE - recuLen - 1, 0);
         if (n <= 0) /* connexion coupée */
         {
            closesocket(e->sock);
            if (e->quitte)
            {
               break; /* on a demandé QUIT : la fermeture est normale */
            }
            if (!e->dejaConnecte)
            {
               printf("\n");
               echec("Le serveur a fermé la connexion.");
               exit(EXIT_FAILURE);
            }
            if (!reconnecter(e, adresse, port))
            {
               echec("Impossible de se reconnecter au serveur. Au revoir.");
               exit(EXIT_FAILURE);
            }
            recuLen = 0; /* un morceau de ligne de l'ancienne connexion ne veut plus rien dire */
            continue;
         }
         recuLen += n;

         /* TCP ne garde pas les frontières des messages : on ne traite que les lignes complètes */
         effacerInvite();
         char *fin;
         while ((fin = memchr(recu, '\n', recuLen)) != NULL)
         {
            *fin = '\0';
            if (fin > recu && *(fin - 1) == '\r')
            {
               *(fin - 1) = '\0';
            }
            traiterMessage(e, recu);
            int longueur = (fin - recu) + 1;
            memmove(recu, recu + longueur, recuLen - longueur);
            recuLen -= longueur;
         }
         if (recuLen == BUF_SIZE - 1)
         {
            recuLen = 0; /* ligne trop longue : on l'abandonne plutôt que de bloquer */
         }
      }
      afficherInvite(e);
   }

   closesocket(e->sock);
}

int main(int argc, char **argv)
{
   /* ./client [adresse [pseudo [port]]] */
   const char *adresse = (argc > 1) ? argv[1] : "localhost";
   const char *pseudo = (argc > 2 && argv[2][0] != '\0') ? argv[2] : NULL;
   int port = (argc > 3) ? atoi(argv[3]) : PORT_DEFAUT;
   if (port <= 0 || port > 65535)
   {
      printf("Usage : %s [adresse [pseudo [port]]]   (par défaut : localhost, pseudo demandé, port %d)\n",
             argv[0], PORT_DEFAUT);
      return EXIT_FAILURE;
   }

   init();

   app(adresse, port, pseudo);

   end();

   return EXIT_SUCCESS;
}
