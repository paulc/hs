
CC ?= gcc
AR ?= ar
CFLAGS = -Wall -Werror -O2 -std=gnu99
LDFLAGS = 
DEBUG ?= -g -rdynamic -ggdb

OBJ = pipe_fd.o anet.o blowfish.o blf_keygen.o
LIB = 
PROGS = hs

all : $(PROGS) $(LIB)

# Deps (from 'make dep')
anet.o: anet.c fmacros.h anet.h
blf_keygen.o: blf_keygen.c blowfish.h
blowfish.o: blowfish.c blowfish.h
hs.o: hs.c pipe_fd.h
pipe_fd.o: pipe_fd.c

# Targets

hs : hs.o $(OBJ)
	$(CC) -o hs $(LDFLAGS) $(DEBUG) hs.o $(OBJ)

# Generic build targets
.c.o:
	$(CC) -c $(CFLAGS) $(DEBUG) $<

dep:
	$(CC) -MM *.c

clean:
	rm -rf $(PROGS) $(LIB) *.o *~ 

