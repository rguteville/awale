#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "affichage.h"

/* Codes ANSI : des suites de caractères que le terminal interprète au lieu de les afficher.
   "\033[32m" passe le texte en vert, "\033[0m" revient à la normale.
   Marche dans les terminaux Mac, Linux et WSL. */
static const char *CODES[NB_COULEURS] = {
    "\033[0m",  /* C_RESET */
    "\033[1m",  /* C_GRAS */
    "\033[2m",  /* C_PALE */
    "\033[31m", /* C_ROUGE */
    "\033[32m", /* C_VERT */
    "\033[33m", /* C_JAUNE */
    "\033[34m", /* C_BLEU */
    "\033[35m", /* C_MAGENTA */
    "\033[36m", /* C_CYAN */
    "\033[7m",  /* C_INVERSE */
};

static int couleurs = 0;    /* 1 si on utilise les couleurs */
static int interactif = 0;  /* 1 si un humain tape au clavier : on affiche alors l'invite */

void initAffichage(void)
{
   /* isatty : 1 si la sortie est un vrai terminal (et pas un fichier ou un tube).
      NO_COLOR : convention répandue pour désactiver les couleurs (https://no-color.org) */
   couleurs = isatty(STDOUT_FILENO) && getenv("NO_COLOR") == NULL;
   interactif = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
   /* chaque ligne s'affiche tout de suite, même si la sortie est redirigée (./client | tee partie.log) :
      sinon printf garderait le texte en mémoire et un message arriverait avec du retard */
   setvbuf(stdout, NULL, _IOLBF, 0);
}

const char *coul(enum Couleur c)
{
   return couleurs ? CODES[c] : "";
}

void sonnerie(void)
{
   if (interactif)
   {
      printf("\a");
   }
}

/* ------------------------------------------------------------------ messages */

static void ligne(enum Couleur c, const char *symbole, const char *format, va_list args)
{
   printf("  %s%s%s ", coul(c), symbole, coul(C_RESET));
   vprintf(format, args);
   printf("\n");
}

void succes(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   ligne(C_VERT, "✔", format, args);
   va_end(args);
}

void echec(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   ligne(C_ROUGE, "✖", format, args);
   va_end(args);
}

void info(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   printf("  %s• ", coul(C_PALE));
   vprintf(format, args);
   printf("%s\n", coul(C_RESET));
   va_end(args);
}

void alerte(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   ligne(C_JAUNE, "!", format, args);
   va_end(args);
}

void texte(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   printf("    ");
   vprintf(format, args);
   printf("\n");
   va_end(args);
}

void titre(const char *format, ...)
{
   char t[BUF_SIZE];
   va_list args;
   va_start(args, format);
   vsnprintf(t, sizeof t, format, args);
   va_end(args);
   printf("\n  %s── %s %s", coul(C_CYAN), t, coul(C_RESET));
   /* complète le trait jusqu'à ~60 colonnes (les accents comptent pour 2 octets, d'où l'approximation) */
   int reste = 56 - (int)strlen(t);
   printf("%s", coul(C_CYAN));
   for (int k = 0; k < reste; k++)
   {
      printf("─");
   }
   printf("%s\n", coul(C_RESET));
}

void afficherBanniere(const char *adresse, int port)
{
   printf("\n");
   printf("  %s╔══════════════════════════════════════╗%s\n", coul(C_CYAN), coul(C_RESET));
   printf("  %s║%s      %sA W A L É%s   ·   jeu en réseau   %s║%s\n", coul(C_CYAN), coul(C_RESET),
          coul(C_GRAS), coul(C_RESET), coul(C_CYAN), coul(C_RESET));
   printf("  %s╚══════════════════════════════════════╝%s\n", coul(C_CYAN), coul(C_RESET));
   info("connecté au serveur %s:%d", adresse, port);
}

/* ------------------------------------------------------------------ invite */

void effacerInvite(void)
{
   if (interactif)
   {
      /* \r : retour en début de ligne ; \033[K : efface jusqu'à la fin de la ligne */
      printf("\r\033[K");
   }
}

void afficherInvite(const EtatClient *e)
{
   if (!interactif)
   {
      return;
   }
   const Rejeu *r = &e->rejeu;
   if (!e->identifie)
   {
      printf("%sVotre pseudo ›%s ", coul(C_GRAS), coul(C_RESET));
   }
   else if (r->actif && r->complet)
   {
      printf("%srejeu #%d · coup %d/%d%s  %s[Entrée] suivant · p précédent · q quitter ›%s ",
             coul(C_CYAN), r->id, r->courant, r->nb - 1, coul(C_RESET), coul(C_PALE), coul(C_RESET));
   }
   else if (e->partie != 0 && e->enPause)
   {
      printf("%spartie #%d en pause ›%s ", coul(C_JAUNE), e->partie, coul(C_RESET));
   }
   else if (e->partie != 0 && e->aDernier && e->dernier.joueurCourant == e->monCamp)
   {
      printf("%s%sà vous (1-6) ›%s ", coul(C_GRAS), coul(C_VERT), coul(C_RESET));
   }
   else if (e->partie != 0)
   {
      printf("%stour de %s ›%s ", coul(C_PALE), e->adversaire, coul(C_RESET));
   }
   else if (e->partieObservee != 0)
   {
      printf("%sobserve #%d ›%s ", coul(C_CYAN), e->partieObservee, coul(C_RESET));
   }
   else if (e->defiDe[0] != '\0')
   {
      printf("%s%sdéfi de %s : o / n ›%s ", coul(C_GRAS), coul(C_JAUNE), e->defiDe, coul(C_RESET));
   }
   else if (e->defiA[0] != '\0')
   {
      printf("%sdéfi envoyé à %s ›%s ", coul(C_PALE), e->defiA, coul(C_RESET));
   }
   else
   {
      printf("%s%s ›%s ", coul(C_GRAS), e->monPseudo, coul(C_RESET));
   }
   fflush(stdout); /* pas de '\n' : sans fflush, l'invite resterait dans le tampon de printf */
}

/* ------------------------------------------------------------------ plateau */

int numeroAffiche(int numCase)
{
   /* rangée du bas = camp 0, cases 0..5 de gauche à droite ; rangée du haut = camp 1, cases 11..6 */
   return (numCase < 6) ? numCase + 1 : 12 - numCase;
}

int coupJoue(const struct Jeu *avant, const struct Jeu *apres)
{
   /* le joueur qui vient de jouer a vidé la case jouée. Ses autres cases ne peuvent que gagner des
      graines (les captures ne se font que chez l'adversaire) : c'est la seule qui passe de > 0 à 0 */
   int joueur = avant->joueurCourant;
   int debut = (joueur == 0) ? 0 : 6;
   for (int c = debut; c < debut + 6; c++)
   {
      if (avant->plateau[c] > 0 && apres->plateau[c] == 0)
      {
         return c;
      }
   }
   return -1;
}

/* largeur de la colonne des noms + 4 espaces (marge et flèche) */
static int retrait(int largeur)
{
   return 2 + 2 + largeur + 1;
}

static void ligneNumeros(int largeur)
{
   printf("%*s", retrait(largeur), "");
   for (int n = 1; n <= 6; n++)
   {
      printf("   %s%d%s ", coul(C_GRAS), n, coul(C_RESET));
   }
   printf("\n");
}

static void bordure(int largeur, const char *gauche, const char *milieu, const char *droite)
{
   printf("%*s%s", retrait(largeur), "", gauche);
   for (int k = 0; k < 6; k++)
   {
      printf("────%s", k < 5 ? milieu : droite);
   }
   printf("\n");
}

static void rangee(const struct Jeu *jeu, const struct Jeu *avant, int coup, int camp,
                   const char *etiquette, int largeur, int monCamp)
{
   int aSonTour = (jeu->joueurCourant == camp);
   printf("  %s%s%s", coul(C_VERT), aSonTour ? "▶ " : "  ", coul(C_RESET));
   printf("%s%-*s%s ", camp == monCamp ? coul(C_GRAS) : "", largeur, etiquette, coul(C_RESET));

   for (int k = 0; k < 6; k++)
   {
      int c = (camp == 0) ? k : 11 - k; /* l'ordre d'affichage : 0..5 en bas, 11..6 en haut */
      const char *style = "";
      if (c == coup)
      {
         style = coul(C_INVERSE); /* la case qui vient d'être jouée */
      }
      else if (avant != NULL && avant->plateau[c] > 0 && jeu->plateau[c] == 0)
      {
         style = coul(C_ROUGE); /* capturée */
      }
      else if (avant != NULL && avant->plateau[c] != jeu->plateau[c])
      {
         style = coul(C_JAUNE); /* a reçu des graines */
      }
      printf("│%s %2d %s", style, jeu->plateau[c], coul(C_RESET));
   }
   printf("│\n");
}

void afficherPlateau(const struct Jeu *jeu, const struct Jeu *avant, char noms[2][PSEUDO_MAX + 1], int monCamp)
{
   int coup = (avant != NULL) ? coupJoue(avant, jeu) : -1;

   char etiquette[2][PSEUDO_MAX + 16];
   int largeur = 0;
   for (int k = 0; k < 2; k++)
   {
      snprintf(etiquette[k], sizeof etiquette[k], "%s%s", noms[k], k == monCamp ? " (vous)" : "");
      int l = strlen(etiquette[k]);
      if (l > largeur)
      {
         largeur = l;
      }
   }

   printf("\n");
   if (monCamp != 0) /* le camp 1 joue la rangée du haut : ses numéros au-dessus */
   {
      ligneNumeros(largeur);
   }
   bordure(largeur, "┌", "┬", "┐");
   rangee(jeu, avant, coup, 1, etiquette[1], largeur, monCamp);
   bordure(largeur, "├", "┼", "┤");
   rangee(jeu, avant, coup, 0, etiquette[0], largeur, monCamp);
   bordure(largeur, "└", "┴", "┘");
   if (monCamp != 1) /* le camp 0 joue la rangée du bas : ses numéros au-dessous */
   {
      ligneNumeros(largeur);
   }
   printf("\n  Graines capturées : %s%s%s %s%d%s  ·  %s%s%s %s%d%s   %s(plus de 24 pour gagner)%s\n\n",
          coul(C_GRAS), noms[0], coul(C_RESET), coul(C_JAUNE), jeu->score[0], coul(C_RESET),
          coul(C_GRAS), noms[1], coul(C_RESET), coul(C_JAUNE), jeu->score[1], coul(C_RESET),
          coul(C_PALE), coul(C_RESET));
}
