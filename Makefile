CC=clang-11
CFLAGS=-g -I.

PROG1 := tests

PROG1_OBJS = tests.o lisp.o string_buffer.o

PROG2 := main

PROG2_OBJS = main.o lisp.o string_buffer.o

all: $(PROG1) $(PROG2)

$(PROG1): $(PROG1_OBJS)
	$(CC) -o $@ $^

$(PROG2): $(PROG2_OBJS)
	$(CC) -o $@ $^

clean:
	-rm *.o $(PROG1) $(PROG2)
