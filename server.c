#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

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
   /* the index for the array */
   int actual = 0;
   int max = sock;
   /* an array for all clients */
   Client clients[MAX_CLIENTS];

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
      for(i = 0; i < actual; i++)
      {
         FD_SET(clients[i].sock, &rdfs);
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

         if (actual == MAX_CLIENTS)
         {
            closesocket(csock);
            continue;
         }

         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         Client client = { csock, "", NON_IDENTIFIE, "", 0 };
         clients[actual] = client;
         actual++;
      }
      else
      {
         int i = 0;
         for(i = 0; i < actual; i++)
         {
            /* a client is talking */
            if(FD_ISSET(clients[i].sock, &rdfs))
            {
               Client* client = &clients[i];
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
                  traiterLigne(clients, actual, i, client->buf);
                  int longueur = (fin - client->buf) + 1;
                  memmove(client->buf, client->buf + longueur, client->bufLen - longueur);
                  client->bufLen -= longueur;
               }

               if (client->bufLen == BUF_SIZE - 1)
               {
                  envoyer(clients[i].sock, "ERR SYNTAX ligne trop longue");
                  client->bufLen = 0;
               }
               /* client disconnected */
               if(n == 0)
               {
                  /* on garde le pseudo AVANT remove_client, qui décale le tableau */
                  char nom[PSEUDO_MAX + 1];
                  int etaitIdentifie = client->etat != NON_IDENTIFIE;
                  strcpy(nom, client->name);

                  closesocket(client->sock);
                  remove_client(clients, i, &actual);

                  /* personne ne connaissait un client non identifié : on ne prévient pas */
                  if (etaitIdentifie)
                  {
                     for (int j = 0; j < actual; j++)
                     {
                        if (clients[j].etat != NON_IDENTIFIE)
                        {
                           envoyer(clients[j].sock, "INFO %s s'est deconnecte", nom);
                        }
                     }
                  }
               }
               break;
            }
         }
      }
   }

   clear_clients(clients, actual);
   end_connection(sock);
}

static void clear_clients(Client *clients, int actual)
{
   int i = 0;
   for(i = 0; i < actual; i++)
   {
      closesocket(clients[i].sock);
   }
}

static void remove_client(Client *clients, int to_remove, int *actual)
{
   /* we remove the client in the array */
   memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
   /* number client - 1 */
   (*actual)--;
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
      exit(errno);
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

static void traiterLigne(Client *clients, int actual, int i, char *ligne)
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
   if (clients[i].etat == NON_IDENTIFIE && strcmp(commande, "LOGIN") != 0)
   {
      envoyer(clients[i].sock, "ERR STATE identifiez-vous avec LOGIN <pseudo>");
   }
   else if (strcmp(commande, "LOGIN") == 0)
   {
      cmdLogin(clients, actual, i, args);
   }
   else if (strcmp(commande, "LIST") == 0)
   {
      cmdList(clients, actual, i);
   }
   else
   {
      envoyer(clients[i].sock, "ERR UNKNOWN commande inconnue");
   }
}

static void cmdLogin(Client *clients, int actual, int i, char *args)
{
   if (clients[i].etat != NON_IDENTIFIE)
   {
      envoyer(clients[i].sock, "ERR STATE vous etes deja identifie");
      return;
   }
   if (strlen(args) == 0 || strlen(args) > PSEUDO_MAX)
   {
      envoyer(clients[i].sock, "ERR SYNTAX pseudo de 1 a %d caracteres", PSEUDO_MAX);
      return;
   }
   for (int k = 0; args[k] != '\0'; k++)
   {
      /* cast en unsigned char : isalnum n'accepte pas les char négatifs (octets d'un 'é' par ex.) */
      if (!isalnum((unsigned char)args[k]) && args[k] != '_')
      {
         envoyer(clients[i].sock, "ERR SYNTAX pseudo : lettres, chiffres et _ uniquement");
         return;
      }
   }
   for (int j = 0; j < actual; j++)
   {
      if (strcmp(clients[j].name, args) == 0)
      {
         envoyer(clients[i].sock, "ERR TAKEN pseudo deja utilise");
         return;
      }
   }
   strcpy(clients[i].name, args);
   clients[i].etat = LIBRE;
   envoyer(clients[i].sock, "OK LOGIN %s", clients[i].name);
}

static void cmdList(Client *clients, int actual, int i)
{
   /* une seule ligne : "USERS alice bob ..." */
   char liste[BUF_SIZE];
   int pos = snprintf(liste, sizeof liste, "USERS");
   for (int j = 0; j < actual; j++)
   {
      if (clients[j].etat != NON_IDENTIFIE)
      {
         pos += snprintf(liste + pos, sizeof liste - pos, " %s", clients[j].name);
      }
   }
   envoyer(clients[i].sock, "%s", liste);
}


int main(void)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
