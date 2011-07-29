# vim: tabstop=8 noexpandtab

VERSION := `date +%Y%m%d`

UNAME := $(shell uname)

PREFIX  = /usr/local
MANDIR  = $(PREFIX)/share/man

CC      = gcc

CFLAGS  =
LFLAGS  =

CFLAGS +=   -g -ggdb -pg
#CFLAGS += -ansi
CFLAGS += -Wno-long-long
CFLAGS += -I/opt/local/include
CFLAGS += -DUSE_INTERFACE=1 \
	  -DSTANDALONE=0 \
	  -DUSE_MATH=1 \
	  -DOSX=1 \
	  -DUSE_DL=1 \
	  -DUSE_ERROR_HOOK=1 \
	  -DUSE_ASCII=1

ifeq ($(UNAME),Linux)
CFLAGS += -D_XOPEN_SOURCE=500 
CFLAGS += -D_BSD_SOURCE
LFLAGS +=  -L/usr/local/lib
LFLAGS +=  -lm -ldl
endif

ifeq ($(UNAME),Darwin)
LFLAGS +=  -L/opt/local/lib 
endif

LFLAGS += -levent -levent_extra

INC     =
SRC     =  main.c scheme.c scheme_sqlite.c
#SRC    += dynload.c

OBJ     = $(SRC:.c=.o)
BIN     = ioscheme
LIB     =

default: all
	@echo " === $(BIN) built for $(UNAME) === "

all:	$(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJ) $(BIN) *.o

install: $(BIN)
	install -v $(BIN) $(PREFIX)/bin

install-man:
	install -d $(MANDIR)/man1
	install -m 0644 ioscheme.1 $(MANDIR)/man1

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)

archive:
	git archive --prefix=$(BIN)-$(VERSION)/ HEAD | gzip > $(BIN)-$(VERSION).tar.gz

help:
	groff -man -Tascii ioscheme.1 | less

.PHONY: default clean test all install install-man uninstall archive help

