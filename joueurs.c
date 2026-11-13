#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "joueurs.h"
#include "envoi.h"
#include "parties.h" /* reprendrePartie */

/* Les fonctions cmdXxx ne vérifient pas l'état du client :
   la table de commandes.c ne les appelle que dans les états autorisés. */

int trouverParPseudo(Serveur *s, const char *pseudo)
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

int trouverParSocket(Serveur *s, SOCKET sock)
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

void annoncer(Serveur *s, int sauf, const char *format, ...)
{
   char texte[BUF_SIZE];
   va_list args;
   va_start(args, format);
   vsnprintf(texte, sizeof texte, format, args);
   va_end(args);

   for (int j = 0; j < s->nbClients; j++)
   {
      if (j != sauf && s->clients[j].etat != NON_IDENTIFIE)
      {
         envoyer(s->clients[j].sock, "INFO %s", texte);
      }
   }
}

void cmdLogin(Serveur *s, int i, char *args)
{
   if (strlen(args) == 0 || strlen(args) > PSEUDO_MAX)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX pseudo de 1 a %d caracteres", PSEUDO_MAX);
      return;
   }
   for (int k = 0; args[k] != '\0'; k++)
   {
      /* cast en unsigned char : isalnum n'accepte pas les char négatifs (octets d'un 'é' par ex.).
         Cette vérification protège aussi les noms de fichiers bio_<pseudo>.txt (pas de '/' ni de '..') */
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
   journal("%s se connecte", s->clients[i].name);
   annoncer(s, i, "%s s'est connecte", s->clients[i].name);
   /* ce pseudo avait peut-être une partie en pause : on l'y remet */
   reprendrePartie(s, i);
}

void cmdList(Serveur *s, int i, char *args)
{
   (void)args;
   /* une seule ligne : "USERS alice bob ..." */
   char liste[BUF_SIZE];
   int pos = snprintf(liste, sizeof liste, "USERS");
   for (int j = 0; j < s->nbClients; j++)
   {
      if (s->clients[j].etat != NON_IDENTIFIE)
      {
         ajouterA(liste, sizeof liste, &pos, " %s", s->clients[j].name);
      }
   }
   envoyer(s->clients[i].sock, "%s", liste);
}

void cmdBioAdd(Serveur *s, int i, char *args)
{
   Client *c = &s->clients[i];
   int n = strlen(args);
   if (n == 0)
   {
      envoyer(c->sock, "ERR SYNTAX ligne de bio vide");
      return;
   }
   if (n > BIO_LIGNE_MAX)
   {
      envoyer(c->sock, "ERR SYNTAX ligne de bio trop longue (%d caracteres max)", BIO_LIGNE_MAX);
      return;
   }
   if (c->nbLignesBio == BIO_LIGNES_MAX)
   {
      envoyer(c->sock, "ERR FULL bio pleine (%d lignes)", BIO_LIGNES_MAX);
      return;
   }
   snprintf(c->bio[c->nbLignesBio], sizeof c->bio[c->nbLignesBio], "%s", args);
   c->nbLignesBio++;
   sauverBio(c->name, c->bio, c->nbLignesBio);
   envoyer(c->sock, "OK BIOADD");
}

void cmdBioClear(Serveur *s, int i, char *args)
{
   (void)args;
   s->clients[i].nbLignesBio = 0;
   sauverBio(s->clients[i].name, s->clients[i].bio, 0); /* fichier vidé */
   envoyer(s->clients[i].sock, "OK BIOCLEAR");
}

void cmdBio(Serveur *s, int i, char *args)
{
   int j = (args[0] == '\0') ? i : trouverParPseudo(s, args);
   if (j == -1)
   {
      envoyer(s->clients[i].sock, "ERR NOTFOUND pseudo inconnu");
      return;
   }
   for (int k = 0; k < s->clients[j].nbLignesBio; k++)
   {
      envoyer(s->clients[i].sock, "BIOLINE %s %s", s->clients[j].name, s->clients[j].bio[k]);
   }
   envoyer(s->clients[i].sock, "BIOEND %s", s->clients[j].name);
}

void cmdMsg(Serveur *s, int i, char *args)
{
   char *texte = strchr(args, ' ');
   if (texte == NULL)
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX usage : MSG <pseudo> <texte>");
      return;
   }
   *texte = '\0';
   texte++;
   if (texte[0] == '\0')
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX message vide");
      return;
   }
   int j = trouverParPseudo(s, args);
   if (j == -1)
   {
      envoyer(s->clients[i].sock, "ERR NOTFOUND pseudo inconnu");
      return;
   }
   envoyer(s->clients[j].sock, "MSG %s %s", s->clients[i].name, texte);
   envoyer(s->clients[i].sock, "OK MSG");
}

void cmdAll(Serveur *s, int i, char *args)
{
   if (args[0] == '\0')
   {
      envoyer(s->clients[i].sock, "ERR SYNTAX message vide");
      return;
   }
   for (int j = 0; j < s->nbClients; j++)
   {
      if (j != i && s->clients[j].etat != NON_IDENTIFIE)
      {
         envoyer(s->clients[j].sock, "ALL %s %s", s->clients[i].name, args);
      }
   }
   envoyer(s->clients[i].sock, "OK ALL");
}
