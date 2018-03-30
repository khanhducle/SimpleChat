# Makefile for CPE464 

CC= gcc
CFLAGS= -g -Wall -Werror

# The  -lsocket -lnsl are sometimes needed for the sockets.
# The -L/usr/ucblib -lucb gives location for the Berkeley library needed for
# the bcopy, bzero, and bcmp.  The -R/usr/ucblib tells where to load
# the runtime library.


all: clean server cclient

server: server.c
	$(CC) $(CFLAGS) server.c networks.c testing.c -o server

cclient: cclient.c
	$(CC) $(CFLAGS) cclient.c networks.c testing.c -o cclient

clean:
	rm -f cclient server



