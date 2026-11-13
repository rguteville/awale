#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

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
#else
   /* Mac/Linux : écrire sur une connexion coupée ne doit pas tuer le client,
      sinon il n'aurait pas le temps de se reconnecter */
   signal(SIGPIPE, SIG_IGN);
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
   etatClient.identifie = 0;
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
         /* connexion coupée */
         if (n <= 0)
         {
            closesocket(sock);
            if (!etatClient.identifie)
            {
               /* pas encore de pseudo accepté : rien à reprendre */
               printf("Server disconnected !\n");
               sock = INVALID_SOCKET;
               break;
            }
            sock = reconnecter(address, &etatClient);
            if (sock == INVALID_SOCKET)
            {
               printf("Impossible de se reconnecter au serveur. Au revoir.\n");
               break;
            }
            recuLen = 0; /* un morceau de ligne de l'ancienne connexion ne veut plus rien dire */
            continue;
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

   if (sock != INVALID_SOCKET)
   {
      end_connection(sock);
   }
}

static int init_connection(const char *address)
{
   /* première connexion : si elle échoue, on arrête le programme */
   SOCKET sock = tenterConnexion(address, 1);
   if (sock == INVALID_SOCKET)
   {
      exit(EXIT_FAILURE);
   }
   return sock;
}

static SOCKET tenterConnexion(const char *address, int afficherErreurs)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = {0};
   struct hostent *hostinfo;

   if (sock == INVALID_SOCKET)
   {
      if (afficherErreurs)
      {
         perror("socket()");
      }
      return INVALID_SOCKET;
   }

   hostinfo = gethostbyname(address);
   if (hostinfo == NULL)
   {
      if (afficherErreurs)
      {
         fprintf(stderr, "Unknown host %s.\n", address);
      }
      closesocket(sock);
      return INVALID_SOCKET;
   }

   sin.sin_addr = *(IN_ADDR *)hostinfo->h_addr_list[0];
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if (connect(sock, (SOCKADDR *)&sin, sizeof(SOCKADDR)) == SOCKET_ERROR)
   {
      if (afficherErreurs)
      {
         perror("connect()");
      }
      closesocket(sock);
      return INVALID_SOCKET;
   }

   return sock;
}

static SOCKET reconnecter(const char *address, EtatClient *e)
{
   printf("Connexion au serveur perdue. Tentative de reconnexion...\n");
   for (int essai = 1; essai <= RECONNEXION_ESSAIS; essai++)
   {
#ifdef WIN32
      Sleep(RECONNEXION_PAUSE * 1000);
#else
      sleep(RECONNEXION_PAUSE);
#endif
      SOCKET sock = tenterConnexion(address, 0); /* 0 : pas de message d'erreur à chaque essai */
      if (sock != INVALID_SOCKET)
      {
         printf("Reconnecté au serveur (essai %d).\n", essai);
         /* on repart de zéro : c'est le serveur qui dira, avec RESUME, si une partie reprend */
         e->identifie = 0;
         e->monCamp = -1;
         e->partieObservee = 0;
         strcpy(e->adversaire, "");
         char ligne[BUF_SIZE];
         snprintf(ligne, sizeof ligne, "LOGIN %s\n", e->monPseudo);
         write_server(sock, ligne);
         return sock;
      }
   }
   return INVALID_SOCKET;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static void write_server(SOCKET sock, const char *buffer)
{
   /* pas d'exit si l'envoi échoue : la coupure sera vue par recv() dans app(),
      qui lancera la reconnexion */
   if (send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
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
   if (strncmp(ligne, "OK LOGIN ", 9) == 0)
   {
      /* le serveur confirme le pseudo : c'est LUI qui fait foi (le joueur a pu en retaper un autre) */
      if (sscanf(ligne, "OK LOGIN %20s", e->monPseudo) == 1)
      {
         e->identifie = 1;
         printf("Connecté en tant que %s. Tapez LIST pour voir les joueurs en ligne.\n", e->monPseudo);
      }
      else
      {
         printf("Erreur lors du parsing de la commande OK LOGIN.\n");
      }
   }
   else if (strncmp(ligne, "START ", 6) == 0)
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
   else if (strncmp(ligne, "RESUME ", 7) == 0)
   {
      /* on s'est reconnecté et le serveur nous remet dans notre partie */
      if (sscanf(ligne, "RESUME %d %20s %d", &id, adv, &camp) == 3)
      {
         e->monCamp = camp;
         snprintf(e->adversaire, sizeof e->adversaire, "%s", adv);
         printf("Partie %d reprise contre %s, vous êtes toujours le joueur %d (%s).\n", id, adv, camp,
                camp == 0 ? "rangée du bas" : "rangée du haut");
      }
      else
      {
         printf("Erreur lors du parsing de la commande RESUME.\n");
      }
   }
   else if (strncmp(ligne, "RESUMED ", 8) == 0)
   {
      if (sscanf(ligne, "RESUMED %d %20s", &id, adv) == 2)
      {
         printf("%s est revenu : la partie %d reprend.\n", adv, id);
      }
      else
      {
         printf("Erreur lors du parsing de la commande RESUMED.\n");
      }
   }
   else if (strncmp(ligne, "PAUSE ", 6) == 0)
   {
      int secondes;
      if (sscanf(ligne, "PAUSE %d %20s %d", &id, adv, &secondes) == 3)
      {
         printf("%s s'est déconnecté : partie %d en pause. S'il ne revient pas dans %d s, la partie se termine.\n",
                adv, id, secondes);
      }
      else
      {
         printf("Erreur lors du parsing de la commande PAUSE.\n");
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
         if (!e->identifie)
         {
            /* pas encore connecté (pseudo pris, invalide...) : le joueur doit renvoyer LOGIN */
            if (strcmp(codeErr, "TAKEN") == 0)
            {
               printf("Ce pseudo est déjà utilisé par un autre joueur.\n");
            }
            printf("Choisissez un pseudo (1 à 20 lettres, chiffres ou _) : tapez LOGIN <pseudo>\n");
         }
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
   else if (strncmp(ligne, "OK REPLAY ", 10) == 0)
   {
      char j0[PSEUDO_MAX + 1], j1[PSEUDO_MAX + 1];
      if (sscanf(ligne, "OK REPLAY %d %20s %20s", &id, j0, j1) == 3)
      {
         e->partieObservee = id;
         printf("Partie %d rejouee : %s (joueur 0) contre %s (joueur 1)\n", id, j0, j1);
      }
      else
      {
         printf("Erreur lors du parsing de la commande OK REPLAY.\n");
      }
   }
   else if (strcmp(ligne, "HISTORY") == 0)
   {
      printf("Aucune partie terminée.\n");
   }
   else if (strncmp(ligne, "HISTORY ", 8) == 0)
   {
      char *mot = strtok(ligne + 8, " ");
      while (mot != NULL)
      {
         char j0[21], j1[21];
         /* %20[^:] : jusqu'à 20 caractères qui ne sont pas ':' */
         if (sscanf(mot, "%d:%20[^:]:%20[^:]:%20s", &id, j0, j1, gagnant) == 4)
         {
            if (strcmp(gagnant, "NUL") == 0)
            {
               printf("partie %d : %s contre %s -> match nul\n", id, j0, j1);
            }
            else
            {
               printf("partie %d : %s contre %s -> victoire de %s\n", id, j0, j1, gagnant);
            }
         }
         else
         {
            printf(" %s\n", mot);
         }
         mot = strtok(NULL, " ");
      }
      printf("Tapez REPLAY <numero> pour revoir une partie.\n");
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
