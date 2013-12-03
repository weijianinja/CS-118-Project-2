CC=gcc

all: server client

server: server.c
	$(CC) server.c -o server

client: client.c
	$(CC) client.c -o client

.PHONY: clean

clean:
	rm client server
