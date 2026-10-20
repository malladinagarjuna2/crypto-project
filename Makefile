CC      := gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -pedantic
CFLAGS  += -Iinclude

SH      := sh
ifeq ($(OS),Windows_NT)
LDLIBS  := -ladvapi32
# PowerShell/cmd usually don't have sh on PATH; fall back to Git for Windows' copy
ifneq ($(wildcard C:/Program\ Files/Git/bin/sh.exe),)
SH      := "C:/Program Files/Git/bin/sh.exe"
endif
endif

LIB_SRC := src/fixed.c src/sha256.c src/hmac.c src/hkdf.c \
           src/keysched.c src/chaos.c src/grid.c src/sbox.c src/raytrace.c \
           src/feistel.c src/mode.c src/rng.c
LIB_OBJ := $(LIB_SRC:.c=.o)

.PHONY: all test check-cli attack clean

ATK_SRC := attack/attack_main.c attack/stats.c attack/sac.c attack/impulse.c attack/known_plaintext.c
ATK_OBJ := $(ATK_SRC:.c=.o)

all: refcrypt.exe tests.exe attack.exe

attack.exe: $(ATK_OBJ) $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS) -lm

attack/%.o: attack/%.c
	$(CC) $(CFLAGS) -Iattack -c -o $@ $<

refcrypt.exe: src/main.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

tests.exe: tests/test_main.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: tests.exe
	./tests.exe

check-cli: refcrypt.exe
	$(SH) tests/cli_test.sh

attack: attack.exe
	./attack.exe

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
#  - SH          : the shell used for check-cli. In PowerShell "sh" is not
#                  on PATH, so on Windows we use Git for Windows' sh.exe.
#  - LIB_SRC     : all the cipher's source files. LIB_OBJ is the same
#                  list with .c changed to .o (compiled object files).
#  - refcrypt.exe: the command-line tool (src/main.c + library).
#  - tests.exe   : the unit tests (tests/test_main.c + library).
#  - attack.exe  : the cryptanalysis suite (attack/*.c + library, -lm
#                  links the math library for log2/erfc/sqrt).
#  - %.o: %.c    : general rule. Compile any .c file into a .o file.
#  - make test      : build and run the unit tests.
#  - make check-cli : run an end-to-end test of refcrypt.exe on real files.
#  - make attack    : run the attack suite (add --mb 10 by running
#                     ./attack.exe --mb 10 for the full 10 MB test).
#  - make clean     : delete all build output.
#
# Usage:  mingw32-make all test check-cli attack
# =====================================================================
