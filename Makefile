# vim: tabstop=8 noexpandtab

DESTDIR =
PREFIX  = /usr/local
MANDIR  = $(PREFIX)/share/man

CC      = gcc

CFLAGS  =  
#CFLAGS += -ansi 
CFLAGS += -Wno-long-long 
CFLAGS += -DUSE_INTERFACE=1 \
	  -DSTANDALONE=0 \
	  -DUSE_MATH=1 \
	  -DOSX=1 \
          -DUSE_DL=1 \
	  -DUSE_ERROR_HOOK=1

LFLAGS  = 

INC     = 
SRC     =  main.c scheme.c 
#SRC    += dynload.c

OBJ     = $(SRC:.c=.o)
BIN     = ioscheme
LIB     = 

default: all

all:	$(BIN) 

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

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
	install -m 0644 ioscheme.3 $(DESTDIR)$(MANDIR)/man3

uninstall:
	rm -f $(DEST)/lib/$(LIB).a
	rm -f $(DEST)/include/$(INC)

.PHONY: default clean test all install install-man uninstall 

