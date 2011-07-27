### most basic makefile for testing Aleph

CC=gcc -std=gnu99
CPPFLAGS=-I.
CFLAGS=-g -Wall
YACC=yacc

SRC=classes.c globals.c main.c gc.c basic.c arith.c symbols.c
OBJ=$(SRC:%.c=%.o) gram.tab.o

all: aleph
	./aleph

aleph.h: types.h
Rcompat.h: aleph.h

gram.tab.c: gram.y Rcompat.h
	$(YACC) -b gram gram.y

%.o: %.c
	$(CC) -o $@ -c $< $(CPPFLAGS) $(CFLAGS)

aleph: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS) $(LIBS)

clean:
	rm -f gram.tab.* $(OBJ) aleph *~


classes.o: classes.c types.h
globals.o: globals.c aleph.h types.h
gram.tab.o: gram.tab.c Rcompat.h aleph.h types.h
main.o: main.c aleph.h types.h Rcompat.h
gc.o: gc.c aleph.h types.h
basic.o: aleph.h types.h
arith.c: aleph.h types.h
symbols.c: types.h
