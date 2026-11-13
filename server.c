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
   if (err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#else
   signal(SIGPIPE, SIG_IGN); /* Mac/Linux : écrire à un client parti ne doit pas tuer le serveur */
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
   s->prochainId = lireDernierId() + 1;

   fd_set rdfs;

   while (1)
   {
      int i = 0;
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the connection socket */
      FD_SET(sock, &rdfs);

      /* add socket of each client */
      for (i = 0; i < s->nbClients; i++)
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
         /* délai écoulé, aucune socket prête : rien d'autre à faire ce tour-ci */
         continue;
      }

      /* something from standard input : i.e keyboard */
      if (FD_ISSET(STDIN_FILENO, &rdfs))
      {
         /* stop process when type on keyboard */
         break;
      }
      else if (FD_ISSET(sock, &rdfs))
      {
         /* new client */
         SOCKADDR_IN csin = {0};
         socklen_t sinsize = sizeof csin;
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         if (csock == SOCKET_ERROR)
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

         Client client = {0};
         client.sock = csock;
         client.etat = NON_IDENTIFIE;
         client.adversaire = INVALID_SOCKET;
         client.partie = -1;

         s->clients[s->nbClients] = client;
         s->nbClients++;
      }
      else
      {
         int i = 0;
         for (i = 0; i < s->nbClients; i++)
         {
            /* a client is talking */
            if (FD_ISSET(s->clients[i].sock, &rdfs))
            {
               Client *client = &s->clients[i];
               int n = recv(client->sock, client->buf + client->bufLen, BUF_SIZE - client->bufLen - 1, 0);
               if (n < 0)
               {
                  perror("recv()");
                  n = 0;
               }
               client->bufLen += n;
               while (1)
               {
                  char *fin = memchr(client->buf, '\n', client->bufLen);
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
               if (n == 0)
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
                     /* pas de défaite immédiate : la partie attend DELAI_RECONNEXION secondes */
                     suspendrePartie(s, i);
                  }
                  if (s->clients[i].etat == OBSERVATEUR)
                  {
                     retirerObservateur(s, i);
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
   for (i = 0; i < s->nbClients; i++)
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
   SOCKADDR_IN sin = {0};

   if (sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   int oui = 1;
   setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&oui, sizeof oui);

   if (bind(sock, (SOCKADDR *)&sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if (listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
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
   if (send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
   }
}

static void envoyer(SOCKET sock, const char *format, ...)
{
   /* joueur déconnecté (partie en pause) : il n'y a personne à qui écrire */
   if (sock == INVALID_SOCKET)
   {
      return;
   }
   char ligne[BUF_SIZE];
   va_list args;

   va_start(args, format);
   vsnprintf(ligne, sizeof ligne - 1, format, args); /* -1 : on garde la place du '\n' */
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
   char *commande = ligne;
   char *args = strchr(ligne, ' ');
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
   else if (strcmp(commande, "GAMES") == 0)
   {
      cmdGames(s, i);
   }
   else if (strcmp(commande, "WATCH") == 0)
   {
      cmdWatch(s, i, args);
   }
   else if (strcmp(commande, "UNWATCH") == 0)
   {
      cmdUnwatch(s, i);
   }
   else if (strcmp(commande, "BIOADD") == 0)
   {
      cmdBioAdd(s, i, args);
   }
   else if (strcmp(commande, "BIOCLEAR") == 0)
   {
      cmdBioClear(s, i);
   }
   else if (strcmp(commande, "BIO") == 0)
   {
      cmdBio(s, i, args);
   }
   else if (strcmp(commande, "MSG") == 0)
   {
      cmdMsg(s, i, args);
   }
   else if (strcmp(commande, "ALL") == 0)
   {
      cmdAll(s, i, args);
   }
   else if (strcmp(commande, "REPLAY") == 0)
   {
      cmdReplay(s, i, args);
   }
   else if (strcmp(commande, "FRIEND") == 0)
   {
      cmdFriend(s, i, args);
   }
   else if (strcmp(commande, "UNFRIEND") == 0)
   {
      cmdUnfriend(s, i, args);
   }
   else if (strcmp(commande,"PRIVATE") == 0)
   {
      cmdPrivate(s, i, args);
   }
   else if (strcmp(commande, "FRIENDS") == 0)
   {
      cmdFriends(s, i);
   }
   else if (strcmp(commande, "HISTORY") == 0)
   {
      cmdHistory(s, i, args);
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
   /* bio sauvegardée lors d'une connexion précédente (0 ligne si aucun fichier) */
   s->clients[i].nbLignesBio = chargerBio(s->clients[i].name, s->clients[i].bio, BIO_LIGNES_MAX);
   s->clients[i].etat = LIBRE;
   envoyer(s->clients[i].sock, "OK LOGIN %s", s->clients[i].name);
   /* ce pseudo avait peut-être une partie en pause : on l'y remet */
   reprendrePartie(s, i);
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
   partie->deconnecteDepuis[0] = 0;
   partie->deconnecteDepuis[1] = 0;
   partie->nbObservateurs = 0;
   partie->historique.id = partie->id;
   strcpy(partie->historique.joueurs[campDeI], s->clients[i].name);
   strcpy(partie->historique.joueurs[1 - campDeI], s->clients[j].name);
   partie->historique.privee = s->clients[i].prive || s->clients[j].prive;
   partie->historique.nbCoups = 0;

   initJeu(&partie->jeu, 0);

   s->clients[i].partie = partie - s->parties; /* indice de la partie dans le tableau */
   s->clients[j].partie = partie - s->parties;
   s->clients[i].etat = EN_PARTIE;
   s->clients[j].etat = EN_PARTIE;

   envoyer(s->clients[i].sock, "START %d %s %d", partie->id, s->clients[j].name, campDeI);
   envoyer(s->clients[j].sock, "START %d %s %d", partie->id, s->clients[i].name, 1 - campDeI);
   envoyerPlateau(partie);
}

static void formaterPlateau(int id, const struct Jeu *jeu, char *ligne, size_t taille)
{
   int pos = snprintf(ligne, taille, "BOARD %d", id);
   for (int k = 0; k < NB_CASES; k++)
   {
      pos += snprintf(ligne + pos, taille - pos, " %d", jeu->plateau[k]);
   }
   snprintf(ligne + pos, taille - pos, " %d %d %d", jeu->score[0], jeu->score[1], jeu->joueurCourant);
}

static void envoyerPlateau(Partie *partie)
{
   char ligne[BUF_SIZE];
   formaterPlateau(partie->id, &partie->jeu, ligne, sizeof ligne);
   for (int k = 0; k < 2; k++)
   {
      envoyer(partie->joueurs[k], "%s", ligne);
   }
   for (int k = 0; k < partie->nbObservateurs; k++)
   {
      envoyer(partie->observateurs[k], "%s", ligne);
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
   if (partie->deconnecteDepuis[0] != 0 || partie->deconnecteDepuis[1] != 0)
   {
      envoyer(s->clients[i].sock, "ERR PAUSED partie en pause, attendez le retour de votre adversaire");
      return;
   }
   int monCamp = campDuClient(partie, s->clients[i].sock);
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
   partie->historique.coups[partie->historique.nbCoups] = numCaseCliquee;
   partie->historique.nbCoups++;
   envoyerPlateau(partie);
   if (partie->jeu.fini)
   {
      const char *raison;
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
      /* les pseudos des deux camps sont dans l'historique : pas besoin de chercher les clients */
      int g = gagnant(&partie->jeu);
      const char *pseudoGagnant = (g == -1) ? "NUL" : partie->historique.joueurs[g];
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
   /* le gagnant est l'autre camp, même s'il est déconnecté en ce moment (partie en pause) */
   Partie *partie = &s->parties[s->clients[i].partie];
   int monCamp = campDuClient(partie, s->clients[i].sock);
   terminerPartie(s, partie, partie->historique.joueurs[1 - monCamp], raison);
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
      if (partie->joueurs[k] == INVALID_SOCKET)
      {
         /* joueur déconnecté : plus de client à prévenir ni à remettre LIBRE */
         continue;
      }
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
   for (int k = 0; k < partie->nbObservateurs; k++)
   {
      envoyer(partie->observateurs[k], "END %d %s %s", partie->id, gagnant, raison);
      int j = trouverParSocket(s, partie->observateurs[k]);
      if (j != -1)
      {
         s->clients[j].etat = LIBRE;
         s->clients[j].partie = -1;
      }
      else
      {
         fprintf(stderr, "Erreur : observateur %d de la partie %d introuvable dans le tableau des clients\n", k, partie->id);
      }
   }
   partie->nbObservateurs = 0;
   strcpy(partie->historique.gagnant, gagnant);
   strcpy(partie->historique.raison, raison);
   sauverPartie(&partie->historique);
   partie->deconnecteDepuis[0] = 0;
   partie->deconnecteDepuis[1] = 0;
   partie->active = 0;
}

static int campDuClient(const Partie *partie, SOCKET sock)
{
   return (partie->joueurs[0] == sock) ? 0 : 1;
}

static void prevenirPartie(const Partie *partie, const char *ligne)
{
   /* envoyer() ignore INVALID_SOCKET : un joueur déconnecté est simplement sauté */
   for (int k = 0; k < 2; k++)
   {
      envoyer(partie->joueurs[k], "%s", ligne);
   }
   for (int k = 0; k < partie->nbObservateurs; k++)
   {
      envoyer(partie->observateurs[k], "%s", ligne);
   }
}

static void suspendrePartie(Serveur *s, int i)
{
   Partie *partie = &s->parties[s->clients[i].partie];
   int camp = campDuClient(partie, s->clients[i].sock);

   /* la partie ne connaît plus la socket de ce joueur : elle le retrouvera par son PSEUDO
      (partie->historique.joueurs[camp]) quand il refera LOGIN */
   partie->joueurs[camp] = INVALID_SOCKET;
   partie->deconnecteDepuis[camp] = time(NULL);

   s->clients[i].etat = LIBRE;
   s->clients[i].partie = -1;
   s->clients[i].adversaire = INVALID_SOCKET;

   char ligne[BUF_SIZE];
   snprintf(ligne, sizeof ligne, "PAUSE %d %s %d", partie->id, partie->historique.joueurs[camp], DELAI_RECONNEXION);
   prevenirPartie(partie, ligne);
}

static void reprendrePartie(Serveur *s, int i)
{
   Client *client = &s->clients[i];
   for (int k = 0; k < MAX_PARTIES; k++)
   {
      Partie *partie = &s->parties[k];
      if (!partie->active)
      {
         continue;
      }
      for (int camp = 0; camp < 2; camp++)
      {
         if (partie->joueurs[camp] != INVALID_SOCKET || strcmp(partie->historique.joueurs[camp], client->name) != 0)
         {
            continue;
         }
         char ligne[BUF_SIZE];

         /* prévenir les autres AVANT de rattacher la socket : celui qui revient reçoit RESUME à la place */
         snprintf(ligne, sizeof ligne, "RESUMED %d %s", partie->id, client->name);
         prevenirPartie(partie, ligne);

         partie->joueurs[camp] = client->sock;
         partie->deconnecteDepuis[camp] = 0;
         client->etat = EN_PARTIE;
         client->partie = k;
         client->adversaire = partie->joueurs[1 - camp]; /* INVALID_SOCKET si l'autre est absent aussi */

         /* l'adversaire (s'il est là) gardait l'ANCIENNE socket : on la remplace */
         int j = trouverParSocket(s, partie->joueurs[1 - camp]);
         if (j != -1)
         {
            s->clients[j].adversaire = client->sock;
         }

         envoyer(client->sock, "RESUME %d %s %d", partie->id, partie->historique.joueurs[1 - camp], camp);
         formaterPlateau(partie->id, &partie->jeu, ligne, sizeof ligne);
         envoyer(client->sock, "%s", ligne);

         /* l'adversaire est peut-être lui aussi déconnecté : la pause continue */
         if (partie->deconnecteDepuis[1 - camp] != 0)
         {
            int reste = DELAI_RECONNEXION - (int)difftime(time(NULL), partie->deconnecteDepuis[1 - camp]);
            envoyer(client->sock, "PAUSE %d %s %d", partie->id, partie->historique.joueurs[1 - camp], reste > 0 ? reste : 0);
         }
         return; /* un pseudo est dans au plus une partie */
      }
   }
}

static void verifierDelais(Serveur *s)
{
   time_t maintenant = time(NULL);
   for (int k = 0; k < MAX_PARTIES; k++)
   {
      Partie *partie = &s->parties[k];
      if (!partie->active)
      {
         continue;
      }
      for (int camp = 0; camp < 2; camp++)
      {
         if (partie->deconnecteDepuis[camp] != 0 &&
             difftime(maintenant, partie->deconnecteDepuis[camp]) >= DELAI_RECONNEXION)
         {
            /* l'autre joueur est absent lui aussi : match nul. Sinon il gagne. */
            const char *pseudoGagnant = (partie->deconnecteDepuis[1 - camp] != 0)
                                            ? "NUL"
                                            : partie->historique.joueurs[1 - camp];
            terminerPartie(s, partie, pseudoGagnant, "DECONNEXION");
            break; /* la partie est terminée : on ne regarde pas l'autre camp */
         }
      }
   }
}

static void retirerObservateur(Serveur *s, int i)
{
   Partie *partie = &s->parties[s->clients[i].partie];
   for (int k = 0; k < partie->nbObservateurs; k++)
   {
      if (partie->observateurs[k] == s->clients[i].sock)
      {
         partie->observateurs[k] = partie->observateurs[partie->nbObservateurs - 1];
         partie->nbObservateurs--;
         break;
      }
   }
   s->clients[i].etat = LIBRE;
   s->clients[i].partie = -1;
}

static void cmdGames(Serveur *s, int i)
{
   char ligne[BUF_SIZE];
   int pos = snprintf(ligne, sizeof ligne, "GAMES");
   for (int k = 0; k < MAX_PARTIES; k++)
   {
      if (s->parties[k].active)
      {
         /* pseudos pris dans l'historique : une partie en pause (joueur absent) est aussi listée */
         pos += snprintf(ligne + pos, sizeof ligne - pos, " %d:%s:%s", s->parties[k].id,
                         s->parties[k].historique.joueurs[0], s->parties[k].historique.joueurs[1]);
      }
   }
   envoyer(s->clients[i].sock, "%s", ligne);
}

static int trouverPartieParId(Serveur *s, int idPartie)
{
   for (int k = 0; k < MAX_PARTIES; k++)
   {
      if (s->parties[k].active && s->parties[k].id == idPartie)
      {
         return k;
      }
   }
   return -1;
}

static void cmdWatch(Serveur *s, int i, char *args)
{
   if (s->clients[i].etat != LIBRE)
   {
      envoyer(s->clients[i].sock, "ERR STATE vous n'etes pas libre pour observer");
      return;
   }
   int idPartie = atoi(args);
   int k = trouverPartieParId(s, idPartie);
   if (k == -1)
   {
      envoyer(s->clients[i].sock, "ERR NOTFOUND partie %d introuvable", idPartie);
      return;
   }
   Partie *partie = &s->parties[k];

   /* partie privée : seuls les amis d'au moins un des deux joueurs peuvent observer.
      privee est fixé à l'ACCEPT : un joueur privé qui se déconnecte (pause) reste protégé */
   int partiePrivee = partie->historique.privee;
   int autorise = 0;
   for (int camp = 0; camp < 2; camp++)
   {
      int j = trouverParSocket(s, partie->joueurs[camp]);
      if (j == -1)
      {
         continue; /* joueur absent (partie en pause) : sa liste d'amis est perdue de toute façon */
      }
      if (s->clients[j].prive)
      {
         partiePrivee = 1; /* passé en PRIVATE ON pendant la partie */
      }
      if (estAmi(s, j, s->clients[i].name))
      {
         autorise = 1;
      }
   }
   if (partiePrivee && !autorise)
   {
      envoyer(s->clients[i].sock, "ERR DENIED partie %d privee : seuls les amis des joueurs peuvent l'observer", idPartie);
      return;
   }

   if (partie->nbObservateurs >= MAX_OBSERVATEURS)
   {
      envoyer(s->clients[i].sock, "ERR FULL trop d'observateurs pour la partie %d", idPartie);
      return;
   }
   partie->observateurs[partie->nbObservateurs] = s->clients[i].sock;
   partie->nbObservateurs++;
   s->clients[i].etat = OBSERVATEUR;
   s->clients[i].partie = k;
   envoyer(s->clients[i].sock, "OK WATCH %d", idPartie);
   
   char ligne[BUF_SIZE];
   formaterPlateau(partie->id, &partie->jeu, ligne, sizeof ligne);
   envoyer(s->clients[i].sock, "%s", ligne);
}

static void cmdUnwatch(Serveur *s, int i)
{
   if (s->clients[i].etat == OBSERVATEUR)
   {
      retirerObservateur(s, i);
      envoyer(s->clients[i].sock, "OK UNWATCH");
   }
   else
   {
      envoyer(s->clients[i].sock, "ERR STATE vous n'etes pas observateur");
   }
}

static void cmdBioAdd(Serveur *s, int i, char *args)
{
   int n = strlen(args);
   if (n == 0)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX ligne de bio vide");
      return;
   }
   if (n > BIO_LIGNE_MAX)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX ligne de bio trop longue (100 max)");
      return;
   }
   if (s->clients[i].nbLignesBio == BIO_LIGNES_MAX)
   {
      envoyer(s->clients[i].sock, "ERR FULL bio pleine (10 lignes)");
      return;
   }
   snprintf(s->clients[i].bio[s->clients[i].nbLignesBio], sizeof(s->clients[i].bio[s->clients[i].nbLignesBio]), "%s", args);
   s->clients[i].nbLignesBio++;
   sauverBio(s->clients[i].name, s->clients[i].bio, s->clients[i].nbLignesBio);
   envoyer(s->clients[i].sock, "OK BIOADD");
}

static void cmdBioClear(Serveur *s, int i)
{
   s->clients[i].nbLignesBio = 0;
   sauverBio(s->clients[i].name, s->clients[i].bio, 0); /* fichier vidé */
   envoyer(s->clients[i].sock, "OK BIOCLEAR");
}

static void cmdBio(Serveur *s, int i, char *args)
{
   int j = (strcmp(args, "") == 0) ? i : trouverParPseudo(s, args);
   if (j != -1)
   {
      for (int k = 0; k < s->clients[j].nbLignesBio; k++)
      {
         envoyer(s->clients[i].sock, "BIOLINE %s %s", s->clients[j].name, s->clients[j].bio[k]);
      }
      envoyer(s->clients[i].sock, "BIOEND %s", s->clients[j].name);
   }
   else
   {
      envoyer(s->clients[i].sock, "ERR NOTFOUND pseudo inconnu");
   }
}

static void cmdMsg(Serveur *s, int i, char *args)
{
   char * texte = strchr(args,' ');
   if (texte==NULL) 
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX usage : MSG <pseudo> <texte>");
      return; 
   }
   *texte = '\0';
   texte++;
   if (texte[0]=='\0')
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX message vide");
      return;
   }
   int j=trouverParPseudo(s,args);
   if (j==-1){
      envoyer(s->clients[i].sock, "ERR NOTFOUND pseudo inconnu");
      return;

   }
   envoyer(s->clients[j].sock, "MSG %s %s", s->clients[i].name, texte);
   envoyer(s->clients[i].sock, "OK MSG");
}

static void cmdAll(Serveur *s, int i, char *args)
{
   if (args[0]=='\0') 
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX message vide");
      return;
   } 
   for (int j=0; j< s->nbClients;j++)
   {
      if (j!=i && s->clients[j].etat!= NON_IDENTIFIE) 
      {
         envoyer(s->clients[j].sock, "ALL %s %s", s->clients[i].name, args);

      }

   }

   envoyer(s->clients[i].sock, "OK ALL");

}

static void cmdReplay(Serveur *s, int i, char *args)
{
   if (s->clients[i].etat != LIBRE)
   {
      envoyer(s->clients[i].sock, "ERR STATE");
      return;
   }
   int id = atoi(args);
   if (id <= 0)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX usage : REPLAY <id>");
      return;
   }
   PartieSauvee partieSauvee;
   if (chercherPartie(id, &partieSauvee) == 0)
   {
      envoyer(s->clients[i].sock, "ERR NOTFOUND partie inconnue");
      return;
   }
   if (partieSauvee.privee)
   {
      /* un joueur revoit toujours SA partie ; sinon il faut être ami d'un des deux joueurs.
         La partie est finie : on cherche les joueurs par PSEUDO (ils n'ont plus de socket de partie) */
      int autorise = strcmp(s->clients[i].name, partieSauvee.joueurs[0]) == 0 ||
                     strcmp(s->clients[i].name, partieSauvee.joueurs[1]) == 0;
      for (int camp = 0; camp < 2; camp++)
      {
         int j = trouverParPseudo(s, partieSauvee.joueurs[camp]);
         if (j != -1 && estAmi(s, j, s->clients[i].name))
         {
            autorise = 1;
         }
      }
      if (!autorise)
      {
         envoyer(s->clients[i].sock, "ERR DENIED partie %d privee : seuls les joueurs et leurs amis peuvent la revoir", id);
         return;
      }
   }
   envoyer(s->clients[i].sock, "OK REPLAY %d %s %s", id, partieSauvee.joueurs[0], partieSauvee.joueurs[1]);
   struct Jeu jeu;
   initJeu(&jeu, 0);
   char ligne[BUF_SIZE];
   formaterPlateau(id, &jeu, ligne, sizeof ligne);
   envoyer(s->clients[i].sock, "%s", ligne);
   for (int k = 0; k < partieSauvee.nbCoups; k++)
   {
      if (jouerCoup(&jeu, partieSauvee.coups[k]) < 0)
      {
         envoyer(s->clients[i].sock, "INFO partie %d : fichier de sauvegarde abime", id);
         break;
      }
      formaterPlateau(id, &jeu, ligne, sizeof ligne);
      envoyer(s->clients[i].sock, "%s", ligne);
   }
   envoyer(s->clients[i].sock, "END %d %s %s", id, partieSauvee.gagnant, partieSauvee.raison);
}

static void cmdFriend(Serveur *s, int i, char *args)
{
   
   if (args[0] == '\0' || strcmp(args, s->clients[i].name) == 0)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX usage : FRIEND <pseudo>");
      return;
   }
   int j=trouverParPseudo(s,args);
   if (j==-1)
   {
      envoyer(s->clients[i].sock, "ERR NOTFOUND pseudo inconnu");
      return;
   }
   if (estAmi(s, i, args)) /* déjà dans la liste : OK quand même, sans doublon */
   {
      envoyer(s->clients[i].sock, "OK FRIEND %s", args);
      return;
   }
   /* testé APRÈS estAmi : avec 20 amis, refaire FRIEND d'un ami existant doit répondre OK */
   if (s->clients[i].nbAmis==MAX_AMIS)
   {
      envoyer(s->clients[i].sock, "ERR FULL liste d'amis pleine (20 max)");
      return;
   }
   strcpy(s->clients[i].amis[s->clients[i].nbAmis],args);
   s->clients[i].nbAmis++;
   envoyer(s->clients[i].sock, "OK FRIEND %s", args);
}

static void cmdUnfriend(Serveur *s, int i, char *args)
{
   if (args[0] == '\0')
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX usage : UNFRIEND <pseudo>");
      return;
   }
   /* pas de trouverParPseudo : on doit pouvoir retirer un ami déconnecté */
   for (int k=0;k<s->clients[i].nbAmis;k++)
   {
      if (strcmp(s->clients[i].amis[k],args)==0)
      {
         int dernier = s->clients[i].nbAmis - 1;
         if (k != dernier) 
         {
            strcpy(s->clients[i].amis[k], s->clients[i].amis[dernier]);
         }
         s->clients[i].nbAmis--;
         envoyer(s->clients[i].sock, "OK UNFRIEND %s", args);
         return;
      }
   }
   envoyer(s->clients[i].sock, "ERR NOTFOUND %s n'est pas dans votre liste d'amis", args);
}

static int estAmi(Serveur *s, int i, const char *pseudo)
{
  
   for (int k=0;k<s->clients[i].nbAmis;k++)
   {
      if (strcmp(s->clients[i].amis[k],pseudo)==0)
      {
         return 1;
      }
   }
   return 0;
}

static void cmdPrivate(Serveur *s, int i, char *args)
{

   if (strcmp(args, "OFF") == 0)
   {
      s->clients[i].prive = 0;
      envoyer(s->clients[i].sock, "OK PRIVATE OFF");
   }
   else if (strcmp(args, "ON") == 0)
   {
      s->clients[i].prive = 1;
      envoyer(s->clients[i].sock, "OK PRIVATE ON");
   }
   else
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX usage : PRIVATE OFF|ON");
   }
}

static void cmdFriends(Serveur *s, int i)
{
   
   char liste[BUF_SIZE];
   int pos = snprintf(liste, sizeof liste, "FRIENDS");
   for (int k = 0; k < s->clients[i].nbAmis; k++)
   {
      pos += snprintf(liste + pos, sizeof liste - pos, " %s", s->clients[i].amis[k]);
   }
   envoyer(s->clients[i].sock, "%s", liste);
}
static void cmdHistory(Serveur *s, int i, char *args)
{
   const char *pseudo = (args[0] == '\0') ? s->clients[i].name : args;
   PartieSauvee res[HISTORIQUE_MAX];
   int n = listerParties(pseudo, res, HISTORIQUE_MAX);
   char ligne[BUF_SIZE];
   int pos = snprintf(ligne, sizeof ligne, "HISTORY");
   for (int k = 0; k < n; k++)
   {
      pos += snprintf(ligne + pos, sizeof ligne - pos, " %d:%s:%s:%s", res[k].id, res[k].joueurs[0], res[k].joueurs[1], res[k].gagnant);
   }
   envoyer(s->clients[i].sock, "%s", ligne);
}



int main(void)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
