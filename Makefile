CC=gcc
SRCDIR=./
CFILES=$(wildcard $(SRCDIR)*.c)
INC=
LIB=
LIBDIR= -L lib/
LDFLAGS= -pthread
CFLAGS=-Wall -Wextra -Werror -Wshadow -Wfatal-errors -std=gnu11 -pthread
ODIR=obj/

.PHONY: all clean debug

all: pre-build
all: CFLAGS += -O3
all: LDFLAGS +=
all: server
all: client

debug: CFLAGS +=-DDEBUG -ggdb -Og
debug: LDFLAGS +=
debug: pre-build 
debug: server
debug: client

$(ODIR)%.o: $(SRCDIR)%.c
	$(CC) $(CFLAGS) -c $< -o $@

pre-build:
	@mkdir -p $(ODIR)

server: $(ODIR)server_listener.o $(ODIR)socket_distributor.o $(ODIR)server_main.o
	$(CC) -o $@ $^ $(LDFLAGS)

client: $(ODIR)client.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	@rm -rf $(ODIR)
	@rm -f server client
