
CC ?= gcc
AR ?= ar
CFLAGS = -Wall -Werror -Wno-deprecated-declarations -O2 -std=gnu99
LDFLAGS = 
DEBUG ?= -g -rdynamic -ggdb
INSTALL = install
INSTALL_FLAGS = -o root -g wheel 
INSTALL_DIR = /usr/bin

OBJ = pipe_fd.o anet.o sha256.o
LIB = 
PROGS = hs

all : $(PROGS) $(LIB)

# Deps (from 'make dep')
anet.o: anet.c fmacros.h anet.h
hs.o: hs.c pipe_fd.h anet.h
pipe_fd.o: pipe_fd.c pipe_fd.h

# Targets

hs : hs.o $(OBJ)
	$(CC) -o hs $(LDFLAGS) $(DEBUG) hs.o $(OBJ)

# Generic build targets
.c.o:
	$(CC) -c $(CFLAGS) $(DEBUG) $<

dep:
	$(CC) -MM *.c

install: hs
	$(INSTALL) $(INSTALL_FLAGS) hs $(INSTALL_DIR)

clean:
	rm -rf $(PROGS) $(LIB) *.o *~ 

