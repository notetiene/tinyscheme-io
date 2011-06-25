# vim: tabstop=8 noexpandtab

DESTDIR =
PREFIX  = /usr/local
MANDIR  = $(PREFIX)/share/man

CC      = gcc

CFLAGS  = -Wall -ansi -pedantic 
CFLAGS += -I/usr/local/include/tinyscheme
CFLAGS += -Wno-long-long

LFLAGS  = -L/usr/local/lib
LFLAGS += -fpic -ltinyscheme

INC     = 
SRC     =  main.c

OBJ     = $(SRC:.c=.o)
BIN     = iodispatch
LIB     = 

default: all

all:	$(BIN) 

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $< $(LFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< 

clean:
	rm -f $(OBJ) $(BIN) 

test: $(BIN)
	nc -u 127.0.0.1 8000
	#@[ $$? = 0 ] && echo "PASSED OK."

install: $(LIB)
	install -v -m 0644 $(LIB).a $(DEST)/lib
	install -v -m 0644 $(INC)   $(DEST)/include

install-man:
	install -d $(DESTDIR)$(MANDIR)/man3
	install -m 0644 bits.3 $(DESTDIR)$(MANDIR)/man3

uninstall:
	rm -f $(DEST)/lib/$(LIB).a
	rm -f $(DEST)/include/$(INC)

.PHONY: default clean test all install install-man uninstall 

