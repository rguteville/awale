#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <signal.h>

#include "server.h"

static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
   signal(SIGPIPE, SIG_IGN);
#endif
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

static void app(void)
{
   SOCKET sock = init_connection();
   int max = sock;
   /* tout l'état du serveur. static : il est mis à zéro au démarrage
      (donc toutes les parties ont active = 0) et ne prend pas de place sur la pile */
   static Serveur serveur;
   Serveur *s = &serveur;
   s->nbClients = 0;
   s->prochainId = 1;

   fd_set rdfs;

   while(1)
   {
      int i = 0;
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the connection socket */
      FD_SET(sock, &rdfs);

      /* add socket of each client */
      for(i = 0; i < s->nbClients; i++)
      {
         FD_SET(s->clients[i].sock, &rdfs);
      }

      if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      /* something from standard input : i.e keyboard */
      if(FD_ISSET(STDIN_FILENO, &rdfs))
      {
         /* stop process when type on keyboard */
         break;
      }
      else if(FD_ISSET(sock, &rdfs))
      {
         /* new client */
         SOCKADDR_IN csin = { 0 };
         socklen_t sinsize = sizeof csin;
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         if(csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

         if (s->nbClients == MAX_CLIENTS)
         {
            closesocket(csock);
            continue;
         }

         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         Client client = { csock, "", NON_IDENTIFIE, "", 0, INVALID_SOCKET, -1 };
         s->clients[s->nbClients] = client;
         s->nbClients++;
      }
      else
      {
         int i = 0;
         for(i = 0; i < s->nbClients; i++)
         {
            /* a client is talking */
            if(FD_ISSET(s->clients[i].sock, &rdfs))
            {
               Client* client = &s->clients[i];
               int n = recv(client->sock, client->buf + client->bufLen, BUF_SIZE - client->bufLen - 1, 0);
               if (n < 0)
               {
                  perror("recv()");
                  n = 0;
               }
               client->bufLen += n;
               while (1)
               {
                  char* fin = memchr(client->buf, '\n', client->bufLen);
                  if (fin == NULL)
                  {
                     break;
                  }
                  *fin = '\0';
                  if (fin > client->buf && *(fin - 1) == '\r')
                  {
                     *(fin - 1) = '\0';
                  }
                  traiterLigne(s, i, client->buf);
                  int longueur = (fin - client->buf) + 1;
                  memmove(client->buf, client->buf + longueur, client->bufLen - longueur);
                  client->bufLen -= longueur;
               }

               if (client->bufLen == BUF_SIZE - 1)
               {
                  envoyer(s->clients[i].sock, "ERR SYNTAX ligne trop longue");
                  client->bufLen = 0;
               }
               /* client disconnected */
               if(n == 0)
               {
                  /* on garde le pseudo AVANT remove_client, qui décale le tableau */
                  char nom[PSEUDO_MAX + 1];
                  int etaitIdentifie = client->etat != NON_IDENTIFIE;
                  strcpy(nom, client->name);

                  if (s->clients[i].etat == DEFI_ENVOYE || s->clients[i].etat == DEFI_RECU)
                  {
                     int j = trouverParSocket(s, s->clients[i].adversaire);
                     if (j != -1)
                     {
                        s->clients[j].etat = LIBRE;
                        s->clients[j].adversaire = INVALID_SOCKET;
                        envoyer(s->clients[j].sock, "CANCELLED %s", nom);
                     }
                  }
                  if (s->clients[i].etat == EN_PARTIE)
                  {
                     abandonnerPartie(s, i, "DECONNEXION");
                  }
                  closesocket(client->sock);
                  remove_client(s, i);

                  /* personne ne connaissait un client non identifié : on ne prévient pas */
                  if (etaitIdentifie)
                  {
                     for (int j = 0; j < s->nbClients; j++)
                     {
                        if (s->clients[j].etat != NON_IDENTIFIE)
                        {
                           envoyer(s->clients[j].sock, "INFO %s s'est deconnecte", nom);
                        }
                     }
                  }
               }
               break;
            }
         }
      }
   }

   clear_clients(s);
   end_connection(sock);
}

static void clear_clients(Serveur *s)
{
   int i = 0;
   for(i = 0; i < s->nbClients; i++)
   {
      closesocket(s->clients[i].sock);
   }
}

static void remove_client(Serveur *s, int to_remove)
{
   /* we remove the client in the array */
   memmove(s->clients + to_remove, s->clients + to_remove + 1, (s->nbClients - to_remove - 1) * sizeof(Client));
   /* number client - 1 */
   s->nbClients--;
}

static int init_connection(void)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = { 0 };

   if(sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if(bind(sock,(SOCKADDR *) &sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if(listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static void write_client(SOCKET sock, const char *buffer)
{
   if(send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
   }
}

static void envoyer(SOCKET sock, const char *format, ...)
{
   char ligne[BUF_SIZE];
   va_list args;

   va_start(args, format);
   vsnprintf(ligne, sizeof ligne - 1, format, args);  /* -1 : on garde la place du '\n' */
   va_end(args);
   strcat(ligne, "\n");
   write_client(sock, ligne);
}

static void traiterLigne(Serveur *s, int i, char *ligne)
{
   /* ligne vide (l'utilisateur a juste tapé Entrée) : on l'ignore */
   if (ligne[0] == '\0')
   {
      return;
   }
   char * commande = ligne ;
   char * args = strchr(ligne, ' ');
   if (args != NULL)
   {
      *args = '\0';
      args++;
   }
   else
   {
      args = "";
   }
   if (s->clients[i].etat == NON_IDENTIFIE && strcmp(commande, "LOGIN") != 0)
   {
      envoyer(s->clients[i].sock, "ERR STATE identifiez-vous avec LOGIN <pseudo>");
   }
   else if (strcmp(commande, "LOGIN") == 0)
   {
      cmdLogin(s, i, args);
   }
   else if (strcmp(commande, "LIST") == 0)
   {
      cmdList(s, i);
   }
   else if (strcmp(commande, "CHALLENGE") == 0)
   {
      cmdChallenge(s, i, args);
   }
   else if (strcmp(commande, "ACCEPT") == 0)
   {
      cmdAccept(s, i);
   }
   else if (strcmp(commande, "REFUSE") == 0)
   {
      cmdRefuse(s, i);
   }
   else if (strcmp(commande, "PLAY") == 0)
   {
      cmdPlay(s, i, args);
   }
   else if (strcmp(commande, "FORFEIT") == 0)
   {
      cmdForfeit(s, i);
   }
   else if (strcmp(commande, "CANCEL") == 0)
   {
      cmdCancel(s, i);
   }
   else
   {
      envoyer(s->clients[i].sock, "ERR UNKNOWN commande inconnue");
   }
}

static void cmdLogin(Serveur *s, int i, char *args)
{
   if (s->clients[i].etat != NON_IDENTIFIE)
   {
      envoyer(s->clients[i].sock, "ERR STATE vous etes deja identifie");
      return;
   }
   if (strlen(args) == 0 || strlen(args) > PSEUDO_MAX)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX pseudo de 1 a %d caracteres", PSEUDO_MAX);
      return;
   }
   for (int k = 0; args[k] != '\0'; k++)
   {
      /* cast en unsigned char : isalnum n'accepte pas les char négatifs (octets d'un 'é' par ex.) */
      if (!isalnum((unsigned char)args[k]) && args[k] != '_')
      {
         envoyer(s->clients[i].sock, "ERR SYNTAX pseudo : lettres, chiffres et _ uniquement");
         return;
      }
   }
   for (int j = 0; j < s->nbClients; j++)
   {
      if (strcmp(s->clients[j].name, args) == 0)
      {
         envoyer(s->clients[i].sock, "ERR TAKEN pseudo deja utilise");
         return;
      }
   }
   strcpy(s->clients[i].name, args);
   s->clients[i].etat = LIBRE;
   envoyer(s->clients[i].sock, "OK LOGIN %s", s->clients[i].name);
}

static void cmdList(Serveur *s, int i)
{
   /* une seule ligne : "USERS alice bob ..." */
   char liste[BUF_SIZE];
   int pos = snprintf(liste, sizeof liste, "USERS");
   for (int j = 0; j < s->nbClients; j++)
   {
      if (s->clients[j].etat != NON_IDENTIFIE)
      {
         pos += snprintf(liste + pos, sizeof liste - pos, " %s", s->clients[j].name);
      }
   }
   envoyer(s->clients[i].sock, "%s", liste);
}

static int trouverParPseudo(Serveur *s, const char *pseudo)
{
   for (int i = 0; i < s->nbClients; i++)
   {
      if (strcmp(s->clients[i].name, pseudo) == 0 && s->clients[i].etat != NON_IDENTIFIE)
      {
         return i;
      }
   }
   return -1;
}

static int trouverParSocket(Serveur *s, SOCKET sock)
{
   for (int i = 0; i < s->nbClients; i++)
   {
      if (s->clients[i].sock == sock && s->clients[i].etat != NON_IDENTIFIE)
      {
         return i;
      }
   }
   return -1;
}

static void cmdChallenge(Serveur *s, int i, char *args)
{
   if (s->clients[i].etat != LIBRE)
   {
      envoyer(s->clients[i].sock, "ERR STATE vous n'etes pas libre pour defier");
      return;
   }
   int j = trouverParPseudo(s, args);
   if (j == -1)
   {
      envoyer(s->clients[i].sock, "ERR NOTFOUND pseudo inconnu");
      return;
   }
   if (j == i)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX vous ne pouvez pas vous defier vous-meme");
      return;
   }
   if (s->clients[j].etat != LIBRE)
   {
      envoyer(s->clients[i].sock, "ERR BUSY %s n'est pas libre pour defi", s->clients[j].name);
      return;
   }
   s->clients[i].etat = DEFI_ENVOYE;
   s->clients[i].adversaire = s->clients[j].sock;
   s->clients[j].etat = DEFI_RECU;
   s->clients[j].adversaire = s->clients[i].sock;
   envoyer(s->clients[i].sock, "OK CHALLENGE %s", s->clients[j].name);
   envoyer(s->clients[j].sock, "CHALLENGED %s", s->clients[i].name);
}

static void cmdRefuse(Serveur *s, int i)
{
   if (s->clients[i].etat != DEFI_RECU)
   {
      envoyer(s->clients[i].sock, "ERR STATE vous n'avez pas de defi en attente");
      return;
   }
   int j = trouverParSocket(s, s->clients[i].adversaire);
   if (j == -1)
   {
      envoyer(s->clients[i].sock, "ERR STATE le defi n'est pas valide");
      s->clients[i].etat = LIBRE;
      s->clients[i].adversaire = INVALID_SOCKET;
      return;
   }
   s->clients[i].etat = LIBRE;
   s->clients[j].etat = LIBRE;
   s->clients[i].adversaire = INVALID_SOCKET;
   s->clients[j].adversaire = INVALID_SOCKET;
   envoyer(s->clients[i].sock, "OK REFUSE");
   envoyer(s->clients[j].sock, "REFUSED %s", s->clients[i].name);
}

static void cmdAccept(Serveur *s, int i)
{
   if (s->clients[i].etat != DEFI_RECU)
   {
      envoyer(s->clients[i].sock, "ERR STATE vous n'avez pas de defi en attente");
      return;
   }
   int j = trouverParSocket(s, s->clients[i].adversaire);
   if (j == -1)
   {
      envoyer(s->clients[i].sock, "ERR STATE le defi n'est pas valide");
      s->clients[i].etat = LIBRE;
      s->clients[i].adversaire = INVALID_SOCKET;
      return;
   }
   Partie *partie = NULL;
   for (int k = 0; k < MAX_PARTIES; k++)
   {
      if (s->parties[k].active == 0)
      {
         partie = &s->parties[k];
         break;
      }
   }
   if (partie == NULL)
   {
      envoyer(s->clients[i].sock, "ERR BUSY trop de parties en cours");
      envoyer(s->clients[j].sock, "ERR BUSY trop de parties en cours");
      s->clients[i].etat = LIBRE;
      s->clients[j].etat = LIBRE;
      s->clients[i].adversaire = INVALID_SOCKET;
      s->clients[j].adversaire = INVALID_SOCKET;
      return;
   }
   int campDeI = rand() % 2;
   partie->active = 1;
   partie->id = s->prochainId++;
   partie->joueurs[campDeI] = s->clients[i].sock;
   partie->joueurs[1 - campDeI] = s->clients[j].sock;
   partie->nbObservateurs = 0;
   initJeu(&partie->jeu, 0);
   s->clients[i].partie = partie - s->parties;  /* indice de la partie dans le tableau */
   s->clients[j].partie = partie - s->parties;
   s->clients[i].etat = EN_PARTIE;
   s->clients[j].etat = EN_PARTIE;
   
   envoyer(s->clients[i].sock, "START %d %s %d", partie->id, s->clients[j].name, campDeI);
   envoyer(s->clients[j].sock, "START %d %s %d", partie->id, s->clients[i].name, 1 - campDeI);
   envoyerPlateau(s, partie);
}

static void envoyerPlateau(Serveur *s, Partie *partie)
{
   char ligne[BUF_SIZE];
   int pos = snprintf(ligne, sizeof ligne, "BOARD %d", partie->id);
   for (int k = 0; k < NB_CASES; k++)
   {
      pos += snprintf(ligne + pos, sizeof ligne - pos, " %d", partie->jeu.plateau[k]);
   }
   pos += snprintf(ligne + pos, sizeof ligne - pos, " %d %d %d", partie->jeu.score[0], partie->jeu.score[1], partie->jeu.joueurCourant);
   for (int k = 0; k < 2; k++)
   {
      envoyer(partie->joueurs[k], "%s", ligne);
   }
}

static void cmdPlay(Serveur *s, int i, char *args)
{
   if (s->clients[i].etat != EN_PARTIE)
   {
      envoyer(s->clients[i].sock, "ERR STATE vous n'etes pas en partie");
      return;
   }
   int caseJouee = atoi(args);
   if (caseJouee < 1 || caseJouee > 6)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX case de 1 a 6 uniquement");
      return;
   }
   Partie *partie = &s->parties[s->clients[i].partie];
   int monCamp = (partie->joueurs[0] == s->clients[i].sock) ? 0 : 1;
   if (partie->jeu.joueurCourant != monCamp)
   {
      envoyer(s->clients[i].sock, "ERR NOTYOURTURN ce n'est pas votre tour");
      return;
   }
   int numCaseCliquee = (monCamp == 0) ? (caseJouee - 1) : (12 - caseJouee);
   if (jouerCoup(&partie->jeu, numCaseCliquee) < 0)
   {
      envoyer(s->clients[i].sock, "ERR ILLEGAL coup illegal");
      return;
   }
   envoyerPlateau(s, partie);
   if (partie->jeu.fini)
   {
      const char * raison;
      if (partie->jeu.score[0] > 24 || partie->jeu.score[1] > 24)
      {
         raison = "SCORE";
      }
      else if (partie->jeu.nbCoups >= NB_COUPS_MAX)
      {
         raison = "LIMITE";
      }
      else
      {
         raison = "BLOQUE";
      }
      const char * pseudoGagnant;
      int g = gagnant(&partie->jeu);
      int j = trouverParSocket(s, partie->joueurs[g]);
      pseudoGagnant = (g == -1 || j == -1) ? "NUL" : s->clients[j].name;
      terminerPartie(s, partie, pseudoGagnant, raison);
   }
}

static void cmdForfeit(Serveur *s, int i)
{
   if (s->clients[i].etat != EN_PARTIE)
   {
      envoyer(s->clients[i].sock, "ERR STATE vous n'etes pas en partie");
      return;
   }
   abandonnerPartie(s, i, "ABANDON");
}

static void abandonnerPartie(Serveur *s, int i, const char *raison)
{
   Partie *partie = &s->parties[s->clients[i].partie];
   int j = trouverParSocket(s, s->clients[i].adversaire);
   const char *pseudoGagnant = (j == -1) ? "NUL" : s->clients[j].name;
   terminerPartie(s, partie, pseudoGagnant, raison);
}

static void cmdCancel(Serveur *s, int i)
{
   if (s->clients[i].etat != DEFI_ENVOYE)
   {
      envoyer(s->clients[i].sock, "ERR STATE vous n'avez pas de defi en attente");
      return;
   }
   int j = trouverParSocket(s, s->clients[i].adversaire);
   if (j == -1)
   {
      envoyer(s->clients[i].sock, "ERR STATE le defi n'est pas valide");
      s->clients[i].etat = LIBRE;
      s->clients[i].adversaire = INVALID_SOCKET;
      return;
   }
   s->clients[i].etat = LIBRE;
   s->clients[j].etat = LIBRE;
   s->clients[i].adversaire = INVALID_SOCKET;
   s->clients[j].adversaire = INVALID_SOCKET;
   envoyer(s->clients[i].sock, "OK CANCEL");
   envoyer(s->clients[j].sock, "CANCELLED %s", s->clients[i].name);
}


static void terminerPartie(Serveur *s, Partie *partie, const char *gagnant, const char *raison)
{
   for (int k = 0; k < 2; k++)
   {
      envoyer(partie->joueurs[k], "END %d %s %s", partie->id, gagnant, raison);
      int j = trouverParSocket(s, partie->joueurs[k]);
      if (j != -1)
      {
         s->clients[j].etat = LIBRE;
         s->clients[j].adversaire = INVALID_SOCKET;
         s->clients[j].partie = -1;
      }
      else 
      {
         fprintf(stderr, "Erreur : joueur %d de la partie %d introuvable dans le tableau des clients\n", k, partie->id);
      }
   }
   partie->active = 0;
}


int main(void)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
