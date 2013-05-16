### Makefile for procinfo.

prefix=/usr

CC = gcc -Wall -Wstrict-prototypes

CFLAGS = -O2
LDFLAGS = -s

# If you get lots of `undefined references', you probably need -lncurses
# instead:

LDLIBS = -ltermcap

### Add to taste:

# CFLAGS  = -g 
# LDFLAGS = -g

# CFLAGS += -DPROC_DIR=\"extra2/\"

# CFLAGS += -DDEBUG
# LDLIBS += -ldmalloc

# CFLAGS += -pg
# LDFLAGS = -pg

### End of configurable options.

all:		procinfo

procinfo:	procinfo.o routines.o

install: procinfo procinfo.8 lsdev.pl socklist.pl lsdev.8
	-mkdir -p $(prefix)/bin
	install procinfo $(prefix)/bin/procinfo
	install lsdev.pl $(prefix)/bin/lsdev
	install socklist.pl $(prefix)/bin/socklist
	-mkdir -p $(prefix)/man/man8
	install -m 644  procinfo.8 $(prefix)/man/man8/procinfo.8
	install -m 644  lsdev.8 $(prefix)/man/man8/lsdev.8
	install -m 644  socklist.8 $(prefix)/man/man8/socklist.8

clean:
	rm -f procinfo procinfo.0 *.o *~ out

procinfo.o : procinfo.c procinfo.h 
routines.o : routines.c procinfo.h 
