CC      := gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -pedantic
CFLAGS  += -Iinclude

LIB_SRC := src/fixed.c src/sha256.c src/hmac.c src/hkdf.c
LIB_OBJ := $(LIB_SRC:.c=.o)

.PHONY: all test clean

all: tests.exe

tests.exe: tests/test_main.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: tests.exe
	./tests.exe

clean:
	rm -f src/*.o tests/*.o attack/*.o *.exe

# =====================================================================
# EXPLANATION (in simple words)
# ---------------------------------------------------------------------
# A Makefile tells "make" (here: mingw32-make) how to build the project.
#
#  - CC / CFLAGS : which compiler to use (gcc) and its options:
#        -std=c11  use the C11 language standard
#        -O2       optimize for speed
#        -Wall -Wextra -pedantic  show lots of warnings to catch mistakes
#        -Iinclude look for .h header files in the include/ folder
#  - LIB_SRC     : the shared library source files. LIB_OBJ is the same
#                  list with .c changed to .o (compiled object files).
#  - tests.exe   : linked from the test program plus the library objects.
#  - %.o: %.c    : general rule. Compile any .c file into a .o file.
#  - make test   : build tests.exe and run it.
#  - make clean  : delete all build output.
#
# Usage:  mingw32-make test
# =====================================================================
