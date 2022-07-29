CC=clang-11
CFLAGS=-g -I.

PROG := tests

PROG_OBJS = tests.o lisp.o string_buffer.o

all: $(PROG)

$(PROG): $(PROG_OBJS)
	$(CC) -o $@ $^

clean:
	-rm *.o $(PROG)
