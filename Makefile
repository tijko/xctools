CC = gcc
LDFLAGS = $(shell pkg-config --libs dbus-1) -lpthread
CFLAGS = $(shell pkg-config --cflags dbus-1) -g -Wall -Werror

default:
	$(CC) rpc-broker.c -o rpc-broker $(CFLAGS) $(LDFLAGS)

clean:
	rm rpc-broker
