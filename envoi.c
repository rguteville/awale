#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "envoi.h"
#include "commun.h" /* BUF_SIZE */

static void write_client(SOCKET sock, const char *buffer)
{
   /* pas d'exit si l'envoi échoue : le client est parti, recv() le verra et le serveur continue */
   send(sock, buffer, strlen(buffer), 0);
}

void envoyer(SOCKET sock, const char *format, ...)
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

int ajouterA(char *ligne, size_t taille, int *pos, const char *format, ...)
{
   int place = (int)taille - *pos; /* octets restants, '\0' compris */
   if (place <= 1)
   {
      return 0;
   }
   va_list args;
   va_start(args, format);
   int n = vsnprintf(ligne + *pos, place, format, args);
   va_end(args);
   if (n < 0 || n >= place)
   {
      ligne[*pos] = '\0'; /* ne tenait pas : on efface le morceau tronqué */
      return 0;
   }
   *pos += n;
   return 1;
}

void journal(const char *format, ...)
{
   time_t maintenant = time(NULL);
   char heure[16];
   strftime(heure, sizeof heure, "%H:%M:%S", localtime(&maintenant));
   printf("[%s] ", heure);

   va_list args;
   va_start(args, format);
   vprintf(format, args);
   va_end(args);

   printf("\n");
   fflush(stdout); /* tout de suite, même si la sortie est redirigée vers un fichier */
}
