#ifndef COMMUN_H
#define COMMUN_H

/* Constantes du protocole, partagées par le client et le serveur */

#define PORT_DEFAUT    1977 /* port utilisé si on n'en donne pas sur la ligne de commande */
#define BUF_SIZE       1024 /* une ligne du protocole fait au plus BUF_SIZE - 1 caractères, '\n' compris */

#define PSEUDO_MAX     20
#define BIO_LIGNES_MAX 10
#define BIO_LIGNE_MAX  100

#endif
