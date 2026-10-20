CC      := gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -pedantic
CFLAGS  += -Iinclude

ifeq ($(OS),Windows_NT)
LDLIBS  := -ladvapi32
endif

LIB_SRC := src/fixed.c src/sha256.c src/hmac.c src/hkdf.c \
           src/keysched.c src/chaos.c src/grid.c src/sbox.c src/raytrace.c \
           src/feistel.c src/mode.c src/rng.c
LIB_OBJ := $(LIB_SRC:.c=.o)

.PHONY: all test check-cli clean

all: refcrypt.exe tests.exe

refcrypt.exe: src/main.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

tests.exe: tests/test_main.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: tests.exe
	./tests.exe

check-cli: refcrypt.exe
	sh tests/cli_test.sh

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
#  - LDLIBS      : on Windows, link advapi32 (it provides CryptGenRandom).
#  - LIB_SRC     : all the cipher's source files. LIB_OBJ is the same
#                  list with .c changed to .o (compiled object files).
#  - refcrypt.exe: the command-line tool (src/main.c + library).
#  - tests.exe   : the unit tests (tests/test_main.c + library).
#  - %.o: %.c    : general rule. Compile any .c file into a .o file.
#  - make test      : build and run the unit tests.
#  - make check-cli : run an end-to-end test of refcrypt.exe on real files.
#  - make clean     : delete all build output.
#
# Usage:  mingw32-make all test check-cli
# =====================================================================
