#include <stdio.h>
#include <string.h>

#include "social.h"
#include "joueurs.h" /* trouverParPseudo */
#include "envoi.h"

int estAmi(Serveur *s, int i, const char *pseudo)
{
   for (int k = 0; k < s->clients[i].nbAmis; k++)
   {
      if (strcmp(s->clients[i].amis[k], pseudo) == 0)
      {
         return 1;
      }
   }
   return 0;
}

void cmdFriend(Serveur *s, int i, char *args)
{
   Client *c = &s->clients[i];
   if (args[0] == '\0' || strcmp(args, c->name) == 0)
   {
      envoyer(c->sock, "ERR SYNTAX usage : FRIEND <pseudo> (pas vous-meme)");
      return;
   }
   if (trouverParPseudo(s, args) == -1)
   {
      envoyer(c->sock, "ERR NOTFOUND pseudo inconnu");
      return;
   }
   if (estAmi(s, i, args)) /* déjà dans la liste : OK quand même, sans doublon */
   {
      envoyer(c->sock, "OK FRIEND %s", args);
      return;
   }
   /* testé APRÈS estAmi : avec 20 amis, refaire FRIEND d'un ami existant doit répondre OK */
   if (c->nbAmis == MAX_AMIS)
   {
      envoyer(c->sock, "ERR FULL liste d'amis pleine (%d max)", MAX_AMIS);
      return;
   }
   strcpy(c->amis[c->nbAmis], args); /* sûr : trouverParPseudo a trouvé ce pseudo, il fait donc au plus 20 caractères */
   c->nbAmis++;
   envoyer(c->sock, "OK FRIEND %s", args);
}

void cmdUnfriend(Serveur *s, int i, char *args)
{
   Client *c = &s->clients[i];
   if (args[0] == '\0')
   {
      envoyer(c->sock, "ERR SYNTAX usage : UNFRIEND <pseudo>");
      return;
   }
   /* pas de trouverParPseudo : on doit pouvoir retirer un ami déconnecté */
   for (int k = 0; k < c->nbAmis; k++)
   {
      if (strcmp(c->amis[k], args) == 0)
      {
         /* retrait en recopiant le dernier à sa place : l'ordre n'a pas d'importance.
            Pas de strcpy d'une chaîne sur elle-même (interdit) si c'était déjà le dernier */
         if (k != c->nbAmis - 1)
         {
            strcpy(c->amis[k], c->amis[c->nbAmis - 1]);
         }
         c->nbAmis--;
         envoyer(c->sock, "OK UNFRIEND %s", args);
         return;
      }
   }
   envoyer(c->sock, "ERR NOTFOUND %s n'est pas dans votre liste d'amis", args);
}

void cmdPrivate(Serveur *s, int i, char *args)
{
   if (strcmp(args, "ON") == 0)
   {
      s->clients[i].prive = 1;
      envoyer(s->clients[i].sock, "OK PRIVATE ON");
   }
   else if (strcmp(args, "OFF") == 0)
   {
      s->clients[i].prive = 0;
      envoyer(s->clients[i].sock, "OK PRIVATE OFF");
   }
   else
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX usage : PRIVATE ON|OFF");
   }
}

void cmdFriends(Serveur *s, int i, char *args)
{
   (void)args;
   char liste[BUF_SIZE];
   int pos = snprintf(liste, sizeof liste, "FRIENDS");
   for (int k = 0; k < s->clients[i].nbAmis; k++)
   {
      ajouterA(liste, sizeof liste, &pos, " %s", s->clients[i].amis[k]);
   }
   envoyer(s->clients[i].sock, "%s", liste);
}
