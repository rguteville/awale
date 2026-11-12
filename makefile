CC = cc
CFLAGS = -Wall -Wextra

.PHONY : all test clean

all : server client

server : server.c server.h awale.c awale.h sauvegarde.c sauvegarde.h commun.h
	$(CC) $(CFLAGS) -o server server.c awale.c sauvegarde.c

client : client.c client.h awale.c awale.h commun.h
	$(CC) $(CFLAGS) -o client client.c awale.c

# test_awale.c fait #include "awale.c" : on le compile SEUL, sans awale.c
test_awale : test_awale.c awale.c awale.h
	$(CC) $(CFLAGS) -o test_awale test_awale.c

test : test_awale
	./test_awale

clean :
	rm -f server client test_awale *.o
