PROG := a2m
SRC := a2m.c
CC := cc
CFLAGS += -g -std=c99 -Wall
PREFIX ?= /usr/local

UNAME := $(shell sh -c 'uname -s 2>/dev/null')

ifeq ($(UNAME),Darwin)
	CC := clang
	CFLAGS += -Wunused-result -Wunused-value
	LDFLAGS += -liconv
endif

default:
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $(PROG)

.PHONY: install clean

install:
	install -d 0755 $(PREFIX)/bin/
	install -m 0755 $(PROG) $(PREFIX)/bin/$(PROG)

clean:
	rm -rf $(PROG) $(PROG).dSYM
