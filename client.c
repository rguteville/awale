#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "client.h"

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
#endif
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

static void app(const char *address, const char *name)
{
   SOCKET sock = init_connection(address);
   char buffer[BUF_SIZE];

   fd_set rdfs;
   char ligne[BUF_SIZE];
   snprintf(ligne, sizeof ligne, "LOGIN %s\n", name);
   /* send our name */
   write_server(sock, ligne);

   char recu[BUF_SIZE]; /* octets reçus mais pas encore traités */
   int recuLen = 0;

   while (1)
   {
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the socket */
      FD_SET(sock, &rdfs);

      if (select(sock + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      /* something from standard input : i.e keyboard */
      if (FD_ISSET(STDIN_FILENO, &rdfs))
      {
         fgets(buffer, BUF_SIZE - 1, stdin);
         write_server(sock, buffer);
      }
      else if (FD_ISSET(sock, &rdfs))
      {
         int n = recv(sock, recu + recuLen, BUF_SIZE - recuLen - 1, 0);
         /* server down */
         if (n <= 0)
         {
            printf("Server disconnected !\n");
            break;
         }
         recuLen += n;
         EtatClient etatClient = {name, "", -1};
         while (1)
         {
            char *fin = memchr(recu, '\n', recuLen);
            if (fin == NULL)
            {
               break;
            }
            *fin = '\0';
            if (fin > recu && *(fin - 1) == '\r')
            {
               *(fin - 1) = '\0';
            }
            traiterMessage(&etatClient, recu);
            int longueur = (fin - recu) + 1;
            memmove(recu, recu + longueur, recuLen - longueur);
            recuLen -= longueur;
         }
      }
   }

   end_connection(sock);
}

static int init_connection(const char *address)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = {0};
   struct hostent *hostinfo;

   if (sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   hostinfo = gethostbyname(address);
   if (hostinfo == NULL)
   {
      fprintf(stderr, "Unknown host %s.\n", address);
      exit(EXIT_FAILURE);
   }

   sin.sin_addr = *(IN_ADDR *)hostinfo->h_addr_list[0];
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if (connect(sock, (SOCKADDR *)&sin, sizeof(SOCKADDR)) == SOCKET_ERROR)
   {
      perror("connect()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static void write_server(SOCKET sock, const char *buffer)
{
   if (send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
      exit(errno);
   }
}

static void traiterMessage(EtatClient *e, char *ligne)
{
   int id, camp;
   char gagnant[21];
   char raison[16];
   char adv[21];
   char texte[256];
   if (strncmp(ligne, "START ", 6) == 0)
   {
      if (sscanf(ligne, "START %d %20s %d", &id, adv, &camp) == 3)
      {
         e->monCamp = camp;
         strncpy(e->adversaire, adv, sizeof e->adversaire - 1);
         printf("Partie %d commencée contre %s, vous êtes le joueur %d.", id, adv, camp);
      }
      else
      {
         printf("Erreur lors du parsing de la commande START.\n");
      }
   }
   else if (strncmp(ligne, "BOARD ", 6) == 0)
   {
      struct Jeu jeu;
      char * p = strtok(ligne + 6, " ");
      for (int k = 0; k < 12; k++)
      {
         p = strtok(NULL, " ");
         if (p == NULL)
         {
            printf("Erreur lors du parsing du plateau.\n");
            return;
         }
         jeu.plateau[k] = atoi(p);
      }
      for (int i = 0; i < 2; i++)
      {
         p = strtok(NULL, " ");
         if (p == NULL)
         {
            printf("Erreur lors du parsing du plateau.\n");
            return;
         }
         jeu.score[i] = atoi(p);
      }
      p = strtok(NULL, " ");
      if (p == NULL)
      {
         printf("Erreur lors du parsing du plateau.\n");
         return;
      }
      jeu.joueurCourant = atoi(p);
      printf(" 1 2 3 4 5 6");
      plateauEnTexte(&jeu, &texte, sizeof texte);
      printf("%s", texte);
      if (jeu.joueurCourant == e->monCamp)
      {
         printf("A vous de jouer : PLAY <1-6>");
      }
      else
      {
         printf("C'est au tour de l'adversaire.");
      }
   }
   else if (strncmp(ligne, "END ", 6) == 0)
   {
      if (sscanf(ligne, "END %d %20s %15s", &id, &gagnant, &raison) == 3)
      {
         if (strcmp(gagnant, "NUL") == 0)
         {
            printf("Partie %d terminée : match nul (%s).\n", id, raison);
         }
         else if (strcmp(gagnant, e->monPseudo) == 0)
         {
            printf("Partie %d terminée : vous avez gagné (%s) !\n", id, raison);
         }
         else
         {
            printf("Partie %d terminée : vous avez perdu (%s).\n", id, raison);
         }
         e->monCamp = -1;
      }
      else
      {
         printf("Erreur lors du parsing de la commande END.\n");
      }
   }
   else
   {
      printf("%s\n", ligne);
   }

}


int main(int argc, char **argv)
{
   if (argc < 3)
   {
      printf("Usage : %s [address] [pseudo]\n", argv[0]);
      return EXIT_FAILURE;
   }

   init();

   app(argv[1], argv[2]);

   end();

   return EXIT_SUCCESS;
}
