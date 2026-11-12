# Protocole client / serveur Awalé

## 1. Principes

- Transport : **TCP**, port **1977**.
- Messages en **texte ASCII**, **un message par ligne**, chaque ligne se termine par `\n`.
- Longueur maximale d'une ligne : **1023 caractères** (hors `\n`). Une ligne plus longue est refusée avec `ERR SYNTAX`.
- Format : `COMMANDE arg1 arg2 ...`
  - la commande est en **majuscules** ;
  - les arguments sont séparés par **un espace** ;
  - le **dernier** argument d'une commande de texte (chat, bio) peut contenir des espaces : c'est tout le reste de la ligne.
- Pseudo : **1 à 20 caractères**, uniquement lettres, chiffres et `_`. Pas d'espace, ce qui permet de les mettre à la suite sur une ligne.
- TCP ne conserve pas les frontières entre messages : chaque côté accumule les octets reçus dans un buffer **par connexion** et ne traite que les lignes complètes (terminées par `\n`).

## 2. États d'un client (côté serveur)

| État | Signification |
|---|---|
| `NON_IDENTIFIE` | Connecté, n'a pas encore envoyé `LOGIN` |
| `LIBRE` | Identifié, ni en partie ni en attente |
| `DEFI_ENVOYE` | A défié quelqu'un, attend la réponse |
| `DEFI_RECU` | A été défié, doit accepter ou refuser |
| `EN_PARTIE` | Joue une partie |
| `OBSERVATEUR` | Regarde une partie |

```
NON_IDENTIFIE --LOGIN--> LIBRE
LIBRE --CHALLENGE--> DEFI_ENVOYE --(refus / annulation)--> LIBRE
LIBRE --(reçoit un défi)--> DEFI_RECU --REFUSE--> LIBRE
DEFI_ENVOYE / DEFI_RECU --(acceptation)--> EN_PARTIE --(fin de partie)--> LIBRE
LIBRE --WATCH--> OBSERVATEUR --UNWATCH / fin de la partie--> LIBRE
```

Toute commande envoyée dans un état où elle n'est pas autorisée reçoit `ERR STATE`. L'état du client ne change pas.

## 3. Commandes client → serveur

Colonne « Priorité » : **1** = cœur du sujet (à faire en premier), **2** = fonctionnalités demandées, **3** = bonus.

### Connexion et joueurs

| Commande | États autorisés | Réponse si succès | Erreurs possibles | Prio |
|---|---|---|---|---|
| `LOGIN <pseudo>` | `NON_IDENTIFIE` | `OK LOGIN <pseudo>` | `SYNTAX`, `TAKEN` | 1 |
| `LIST` | tous sauf `NON_IDENTIFIE` | `USERS <p1> <p2> ...` | — | 1 |
| `QUIT` | tous | (le serveur ferme la connexion) | — | 1 |

### Défis et parties

| Commande | États autorisés | Réponse si succès | Erreurs possibles | Prio |
|---|---|---|---|---|
| `CHALLENGE <pseudo>` | `LIBRE` | `OK CHALLENGE <pseudo>` | `NOTFOUND`, `BUSY`, `SYNTAX` | 1 |
| `ACCEPT` | `DEFI_RECU` | `START ...` (aux deux joueurs) | — | 1 |
| `REFUSE` | `DEFI_RECU` | `OK REFUSE` | — | 1 |
| `CANCEL` | `DEFI_ENVOYE` | `OK CANCEL` | — | 2 |
| `PLAY <n>` | `EN_PARTIE` | `BOARD ...` (aux joueurs et observateurs) | `SYNTAX`, `ILLEGAL`, `NOTYOURTURN` | 1 |
| `FORFEIT` | `EN_PARTIE` | `END ...` | — | 2 |

`PLAY <n>` : `n` va de **1 à 6** et désigne la n-ième case **du camp du joueur**, comptée **de gauche à droite sur l'affichage** du plateau. L'affichage est le même pour les deux joueurs : rangée du haut = camp 1 (cases 11 à 6), rangée du bas = camp 0 (cases 0 à 5). C'est le **serveur** qui convertit `n` en numéro de case absolu :

| `n` | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| camp 0 | 0 | 1 | 2 | 3 | 4 | 5 |
| camp 1 | 11 | 10 | 9 | 8 | 7 | 6 |

### Chat

| Commande | États autorisés | Réponse si succès | Erreurs possibles | Prio |
|---|---|---|---|---|
| `MSG <pseudo> <texte>` | tous sauf `NON_IDENTIFIE` | `OK MSG` | `NOTFOUND`, `SYNTAX` | 2 |
| `ALL <texte>` | tous sauf `NON_IDENTIFIE` | `OK ALL` | `SYNTAX` | 2 |

`MSG` fonctionne **pendant et en dehors** d'une partie (par exemple pour parler à son adversaire).

### Observation

| Commande | États autorisés | Réponse si succès | Erreurs possibles | Prio |
|---|---|---|---|---|
| `GAMES` | tous sauf `NON_IDENTIFIE` | `GAMES <id>:<j0>:<j1> ...` | — | 2 |
| `WATCH <id>` | `LIBRE` | `OK WATCH <id>` puis `BOARD ...` | `NOTFOUND`, `DENIED`, `FULL` (trop d'observateurs) | 2 |
| `UNWATCH` | `OBSERVATEUR` | `OK UNWATCH` | — | 2 |

### Bio

| Commande | États autorisés | Réponse si succès | Erreurs possibles | Prio |
|---|---|---|---|---|
| `BIOADD <ligne>` | tous sauf `NON_IDENTIFIE` | `OK BIOADD` | `FULL` (déjà 10 lignes), `SYNTAX` | 2 |
| `BIOCLEAR` | tous sauf `NON_IDENTIFIE` | `OK BIOCLEAR` | — | 2 |
| `BIO <pseudo>` | tous sauf `NON_IDENTIFIE` | `BIOLINE <pseudo> <ligne>` (une par ligne de bio) puis `BIOEND <pseudo>` | `NOTFOUND` | 2 |

Une bio compte au plus **10 lignes** de **1 à 100 caractères**. Elle est conservée tant que le joueur est connecté.

### Mode privé et amis

| Commande | États autorisés | Réponse si succès | Erreurs possibles | Prio |
|---|---|---|---|---|
| `FRIEND <pseudo>` | tous sauf `NON_IDENTIFIE` | `OK FRIEND <pseudo>` | `NOTFOUND` | 2 |
| `UNFRIEND <pseudo>` | tous sauf `NON_IDENTIFIE` | `OK UNFRIEND <pseudo>` | `NOTFOUND` | 2 |
| `PRIVATE ON` / `PRIVATE OFF` | tous sauf `NON_IDENTIFIE` | `OK PRIVATE ON` / `OK PRIVATE OFF` | `SYNTAX` | 2 |

En mode privé, seuls les amis **d'au moins un des deux joueurs** peuvent observer leurs parties. Les autres reçoivent `ERR DENIED`.

### Parties sauvegardées

| Commande | États autorisés | Réponse si succès | Erreurs possibles | Prio |
|---|---|---|---|---|
| `HISTORY` | tous sauf `NON_IDENTIFIE` | `HISTORY <id>:<j0>:<j1>:<gagnant> ...` | — | 3 |
| `REPLAY <id>` | `LIBRE` | une suite de `BOARD ...` puis `END ...` | `NOTFOUND` | 3 |

## 4. Messages serveur → client

Il y a deux sortes de messages :
- les **réponses**, qui arrivent après une commande du client : `OK`, `ERR`, `USERS`, `GAMES`... ;
- les **événements**, qui peuvent arriver **à tout moment** : un défi reçu, le coup de l'adversaire, un message de chat... Le client doit donc toujours être prêt à lire le socket, d'où le `select()`.

| Message | Quand | Signification |
|---|---|---|
| `OK <commande> [détails]` | réponse | La commande a réussi |
| `ERR <code> <texte>` | réponse | La commande a échoué (codes en section 5) |
| `USERS <p1> <p2> ...` | réponse à `LIST` | Joueurs connectés |
| `GAMES <id>:<j0>:<j1> ...` | réponse à `GAMES` | Parties en cours |
| `CHALLENGED <pseudo>` | événement | `<pseudo>` vous défie |
| `REFUSED <pseudo>` | événement | `<pseudo>` a refusé votre défi |
| `CANCELLED <pseudo>` | événement | `<pseudo>` a annulé son défi |
| `START <id> <adversaire> <votre_camp>` | événement | La partie commence ; `<votre_camp>` vaut `0` ou `1` |
| `BOARD <id> <c0> ... <c11> <score0> <score1> <joueur_courant>` | événement | État du plateau, envoyé après chaque coup |
| `END <id> <gagnant> <raison>` | événement | Fin de partie ; `<gagnant>` est un pseudo ou `NUL` |
| `MSG <de> <texte>` | événement | Message privé reçu |
| `ALL <de> <texte>` | événement | Message à tous |
| `BIOLINE <pseudo> <ligne>` / `BIOEND <pseudo>` | réponse à `BIO` | Une ligne de bio / fin de la bio |
| `INFO <texte>` | événement | Information du serveur (« bob s'est connecté »...) |

**Le plateau.** `BOARD` tient sur **une seule ligne** : les 12 cases dans l'ordre absolu 0 à 11, puis les deux scores, puis le joueur dont c'est le tour. C'est le **client** qui le met en forme pour l'affichage.

**Raisons de fin** (`END`) : `SCORE` (plus de 24 graines), `BLOQUE` (plus de coup légal), `LIMITE` (50 coups), `ABANDON`, `DECONNEXION`.

## 5. Codes d'erreur

| Code | Signification |
|---|---|
| `UNKNOWN` | Commande inconnue |
| `SYNTAX` | Mauvais nombre ou format d'arguments, ligne trop longue |
| `STATE` | Commande non autorisée dans l'état actuel (ex. `PLAY` hors partie) |
| `TAKEN` | Pseudo déjà utilisé |
| `NOTFOUND` | Pseudo ou partie inconnu |
| `BUSY` | Le joueur défié n'est pas `LIBRE` |
| `ILLEGAL` | Coup refusé par les règles |
| `NOTYOURTURN` | Ce n'est pas votre tour |
| `DENIED` | Partie privée : vous n'êtes pas ami avec les joueurs |
| `FULL` | Plus de place : bio déjà pleine (10 lignes), ou partie avec déjà trop d'observateurs |

## 6. Règles côté serveur

- Au lancement d'une partie, le serveur **tire au sort** le premier joueur.
- Le serveur est le **seul arbitre** : il vérifie chaque coup avec le moteur (`coupLegal` / `jouerCoup`) et ne fait jamais confiance au client.
- **Déconnexion d'un joueur en partie** : la partie s'arrête, l'adversaire gagne, et tous reçoivent `END <id> <adversaire> DECONNEXION`.
- **Déconnexion avec un défi en attente** : le défi est annulé, l'autre joueur reçoit `CANCELLED <pseudo>`.

## 7. Exemples d'échanges

`C:` = envoyé par le client, `S:` = envoyé par le serveur.

**Connexion et défi refusé**

```
alice  C: LOGIN alice
alice  S: OK LOGIN alice
bob    C: LOGIN bob
bob    S: OK LOGIN bob
alice  C: LIST
alice  S: USERS alice bob
alice  C: CHALLENGE bob
alice  S: OK CHALLENGE bob
bob    S: CHALLENGED alice
bob    C: REFUSE
bob    S: OK REFUSE
alice  S: REFUSED bob
```

**Défi accepté et début de partie** (le tirage au sort donne le camp 0 à bob : il commence)

```
alice  C: CHALLENGE bob
bob    S: CHALLENGED alice
bob    C: ACCEPT
alice  S: START 1 bob 1
bob    S: START 1 alice 0
alice  S: BOARD 1 4 4 4 4 4 4 4 4 4 4 4 4 0 0 0
bob    S: BOARD 1 4 4 4 4 4 4 4 4 4 4 4 4 0 0 0
bob    C: PLAY 1
alice  S: BOARD 1 0 5 5 5 5 4 4 4 4 4 4 4 0 0 1
bob    S: BOARD 1 0 5 5 5 5 4 4 4 4 4 4 4 0 0 1
```

**Erreurs**

```
bob    C: PLAY 2
bob    S: ERR NOTYOURTURN ce n'est pas votre tour
carol  C: PLAY 3
carol  S: ERR STATE vous n'etes pas en partie
carol  C: DANSE
carol  S: ERR UNKNOWN commande inconnue
```

## 8. Améliorations possibles

- Reprise d'une partie après une déconnexion (le joueur se reconnecte avec le même pseudo dans un délai donné).
- Classement Elo, tournois.
