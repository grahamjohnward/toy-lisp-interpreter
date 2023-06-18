CC=clang-13
CFLAGS=-g

PROG1 := tests

PROG1_OBJS = tests.o

PROG2 := main

PROG2_OBJS = main.o

LIBNAME=lisp
LIB=lib$(LIBNAME).a

all: $(PROG1) $(PROG2)

$(LIB): lisp.o compile.o string_buffer.o text_stream.o
	$(AR) rs $@ $^

$(PROG1): $(PROG1_OBJS) $(LIB)
	$(CC) -o $@ $< -L. -l$(LIBNAME)

$(PROG2): $(PROG2_OBJS) $(LIB)
	$(CC) -o $@ $< -L. -l$(LIBNAME)

clean:
	-rm *.o $(PROG1) $(PROG2) $(LIB)
