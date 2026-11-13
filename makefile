CC = cc
CFLAGS = -Wall -Wextra
LDLIBS = -lm   # bibliothèque mathématique (pow, pour l'Elo) : obligatoire sous Linux / WSL

# un .o par fichier .c : après une modification, make ne recompile que ce qui a changé
SERVEUR = server.o commandes.o envoi.o joueurs.o parties.o observation.o social.o archives.o \
          classement.o sauvegarde.o awale.o
CLIENT  = client.o saisie.o messages.o affichage.o awale.o
ENTETES = $(wildcard *.h)

.PHONY : all test clean

all : server client

server : $(SERVEUR)
	$(CC) $(CFLAGS) -o $@ $(SERVEUR) $(LDLIBS)

client : $(CLIENT)
	$(CC) $(CFLAGS) -o $@ $(CLIENT) $(LDLIBS)

# règle générale : x.o se fabrique à partir de x.c ($< = le .c, $@ = le .o).
# Chaque .o dépend de tous les .h : plus simple, et sûr (un .h modifié recompile tout)
%.o : %.c $(ENTETES)
	$(CC) $(CFLAGS) -c $< -o $@

# test_awale.c fait #include "awale.c" : on le compile SEUL, sans awale.c
test_awale : test_awale.c awale.c awale.h
	$(CC) $(CFLAGS) -o test_awale test_awale.c

test : test_awale
	./test_awale

clean :
	rm -f server client test_awale *.o
