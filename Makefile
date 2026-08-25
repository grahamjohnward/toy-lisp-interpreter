ifeq ($(shell uname), Darwin)
  CC=clang
else
  CC=clang-19
endif

ifeq ($(BUILD), debug)
  CFLAGS=-gdwarf-4 -falign-functions=8 -DVM_TRACE_ENABLED
else
  CFLAGS=-gdwarf-4 -falign-functions=8 -DNDEBUG
  OPTFLAGS=-O3
endif

PROG1 := tests

PROG1_OBJS = tests.o

PROG2 := main

PROG2_OBJS = main.o

LIBNAME=lisp
LIB=lib$(LIBNAME).a

all: $(PROG1) $(PROG2)

$(LIB): lisp.o vm.o vm-O.o lexical_scope.o compile.o string_buffer.o text_stream.o
	$(AR) rs $@ $^

%-O.o: %-O.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -c -o $@ $<

$(PROG1): $(PROG1_OBJS) $(LIB)
	$(CC) -o $@ $< -L. -l$(LIBNAME)

$(PROG2): $(PROG2_OBJS) $(LIB)
	$(CC) -o $@ $< -L. -l$(LIBNAME)

clean:
	-rm *.o $(PROG1) $(PROG2) $(LIB)
