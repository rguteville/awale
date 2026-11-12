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

   EtatClient etatClient;
   snprintf(etatClient.monPseudo, sizeof etatClient.monPseudo, "%s", name);
   strcpy(etatClient.adversaire, "");
   etatClient.monCamp = -1;
   etatClient.partieObservee = 0;

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
         if (fgets(buffer, BUF_SIZE - 1, stdin) == NULL)
         {
            printf("Au revoir.\n");
            break;
         }
         else
         {
            write_server(sock, buffer);
         }
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
   char texte[1024];
   char codeErr[16];
   if (strncmp(ligne, "START ", 6) == 0)
   {
      if (sscanf(ligne, "START %d %20s %d", &id, adv, &camp) == 3)
      {
         e->monCamp = camp;
         strncpy(e->adversaire, adv, sizeof e->adversaire - 1);
         char *campStr = (camp == 0) ? "rangée du bas" : "rangée du haut";
         printf("Partie %d commencée contre %s, vous êtes le joueur %d (%s).\n", id, adv, camp, campStr);
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
      printf("1 2 3 4 5 6\n");
      plateauEnTexte(&jeu, texte, sizeof texte);
      printf("%s", texte);
      if (e->partieObservee != 0)
      {
         /* observateur : ni "vous" ni "adversaire", on désigne les camps */
         printf("Au tour du camp %d (%s).\n", jeu.joueurCourant,
                jeu.joueurCourant == 0 ? "rangée du bas" : "rangée du haut");
      }
      else if (jeu.joueurCourant == e->monCamp)
      {
         printf("A vous de jouer : PLAY <1-6>\n");
      }
      else
      {
         printf("C'est au tour de %s.\n", e->adversaire);
      }
   }
   else if (strncmp(ligne, "END ", 4) == 0)
   {
      if (sscanf(ligne, "END %d %20s %15s", &id, gagnant, raison) == 3)
      {
         if (strcmp(gagnant, "NUL") == 0)
         {
            printf("Partie %d terminée : match nul (%s).\n", id, raison);
         }
         else if (e->partieObservee != 0)
         {
            /* observateur : il n'a ni gagné ni perdu */
            printf("Partie %d terminée : victoire de %s (%s).\n", id, gagnant, raison);
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
         e->partieObservee = 0;   /* le serveur nous a remis LIBRE */
      }
      else
      {
         printf("Erreur lors du parsing de la commande END.\n");
      }
   }
   else if (strncmp(ligne, "OK WATCH ", 9) == 0)
   {
      if (sscanf(ligne, "OK WATCH %d", &id) == 1)
      {
         e->partieObservee = id;
         printf("Vous observez la partie %d. Tapez UNWATCH pour arrêter.\n", id);
      }
      else
      {
         printf("Erreur lors du parsing de la commande OK WATCH.\n");
      }
   }
   else if (strcmp(ligne, "OK UNWATCH") == 0)
   {
      printf("Vous n'observez plus la partie %d.\n", e->partieObservee);
      e->partieObservee = 0;
   }
   else if (strcmp(ligne, "GAMES") == 0)
   {
      printf("Aucune partie en cours.\n");
   }
   else if (strncmp(ligne, "GAMES ", 6) == 0)
   {
      /* "GAMES 1:alice:bob 2:carol:dave" : un mot par partie, champs séparés par ':' */
      printf("Parties en cours :\n");
      char *p = strtok(ligne + 6, " ");
      while (p != NULL)
      {
         char j0[21], j1[21];
         /* %20[^:] : jusqu'à 20 caractères qui ne sont pas ':' */
         if (sscanf(p, "%d:%20[^:]:%20s", &id, j0, j1) == 3)
         {
            printf("  partie %d : %s (camp 0) contre %s (camp 1)\n", id, j0, j1);
         }
         else
         {
            printf("  %s\n", p);
         }
         p = strtok(NULL, " ");
      }
      printf("Tapez WATCH <numero> pour observer une partie.\n");
   }
   else if (strncmp(ligne, "CHALLENGED ", 11) == 0)
   {
      if (sscanf(ligne, "CHALLENGED %20s", adv) == 1)
      {
         printf("%s vous défie !\n", adv);
         printf("Tapez ACCEPT ou REFUSE pour accepter ou refuser la partie.\n");
      }
      else
      {
         printf("Erreur lors du parsing de la commande CHALLENGED.\n");
      }
   }
   else if (strncmp(ligne, "REFUSED ", 8) == 0)
   {
      if (sscanf(ligne, "REFUSED %20s", adv) == 1)
      {
         printf("Défi refusé par %s !\n", adv);
      }
      else
      {
         printf("Erreur lors du parsing de la commande REFUSED.\n");
      }
   }
   else if (strncmp(ligne, "CANCELLED ", 10) == 0)
   {
      if (sscanf(ligne, "CANCELLED %20s", adv) == 1)
      {
         printf("%s a annulé le défi.\n", adv);
      }
      else
      {
         printf("Erreur lors du parsing de la commande CANCELLED.\n");
      }
   }
   else if (strncmp(ligne, "MSG ", 4) == 0)
   {
      if (sscanf(ligne, "MSG %20s %1023[^\n]", adv, texte) == 2)
      {
         printf("[privé] %s : %s\n", adv, texte);
      }
      else
      {
         printf("Erreur lors du parsing de la commande MSG.\n");
      }

   }
   else if (strncmp(ligne, "ALL ", 4) == 0)
   {
      if (sscanf(ligne, "ALL %20s %1023[^\n]", adv, texte) == 2)
      {
         printf("[public] %s : %s\n", adv, texte);
      }
      else
      {
         printf("Erreur lors du parsing de la commande ALL.\n");
      }
   }
   else if (strncmp(ligne, "ERR ", 4) == 0)
   {
      if (sscanf(ligne, "ERR %15s %1023[^\n]", codeErr, texte) == 2)
      {
         printf("[ERREUR] : %s, veuillez réessayer.\n", texte);
      }
      else
      {
         printf("Erreur lors du parsing de la commande ERR.\n");
      }
   }
   else if (strncmp(ligne, "BIOLINE ", 8) == 0)
   {
      if (sscanf(ligne, "BIOLINE %20s %1023[^\n]", adv, texte) == 2)
      {
         printf("%s : %s\n", adv, texte);
      }
      else
      {
         printf("Erreur lors du parsing de la commande BIOLINE.\n");
      }
   }
   else if (strncmp(ligne, "BIOEND ", 7) == 0)
   {
      if (sscanf(ligne, "BIOEND %20s", adv) == 1)
      {
         printf("--- fin de la bio de %s ---\n", adv);
      }
      else
      {
         printf("Erreur lors du parsing de la commande BIOEND.\n");
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
