#!/bin/sh
# End-to-end test of refcrypt.exe on real files.
set -u
BIN=./refcrypt.exe
[ -x "$BIN" ] || BIN=./refcrypt
T=tests/tmp
rm -rf "$T"; mkdir -p "$T"
fail=0

check() {   # check <description> <command...>
    desc=$1; shift
    if "$@" >/dev/null 2>&1; then echo "  ok   $desc"; else echo "  FAIL $desc"; fail=1; fi
}
check_fails() {
    desc=$1; shift
    if "$@" >/dev/null 2>&1; then echo "  FAIL $desc (should have failed)"; fail=1; else echo "  ok   $desc"; fi
}

$BIN keygen "$T/a.key" >/dev/null
$BIN keygen "$T/b.key" >/dev/null
check "key file is 32 bytes" test "$(wc -c < "$T/a.key" | tr -d ' ')" = 32

: > "$T/empty.txt"
printf 'Hello, refractive world!\n' > "$T/small.txt"
head -c 200000 /dev/urandom > "$T/random.bin"
for f in empty.txt small.txt random.bin; do
    check "encrypt $f"          $BIN enc "$T/a.key" "$T/$f" "$T/$f.enc"
    check "decrypt $f"          $BIN dec "$T/a.key" "$T/$f.enc" "$T/$f.out"
    check "round trip $f"       cmp -s "$T/$f" "$T/$f.out"
    check "size overhead 48 B"  test $(( $(wc -c < "$T/$f.enc") - $(wc -c < "$T/$f") )) -eq 48
done

$BIN enc "$T/a.key" "$T/small.txt" "$T/small2.enc" >/dev/null
check_fails "same file encrypts differently each time" cmp -s "$T/small.txt.enc" "$T/small2.enc"
check_fails "wrong key is rejected"   $BIN dec "$T/b.key" "$T/small.txt.enc" "$T/x.out"

# flip one bit in the middle of the ciphertext
cp "$T/random.bin.enc" "$T/tampered.enc"
printf '\001' | dd of="$T/tampered.enc" bs=1 seek=1000 count=1 conv=notrunc 2>/dev/null
check_fails "tampered file is rejected" $BIN dec "$T/a.key" "$T/tampered.enc" "$T/x.out"
check "rejected file produced no output" test ! -e "$T/x.out"

check "trace writes CSV" $BIN trace "$T/a.key" 0123456789abcdef "$T/ray.csv"
check "CSV has 50 lines" test "$(wc -l < "$T/ray.csv" | tr -d ' ')" = 50

rm -rf "$T"
if [ $fail -eq 0 ]; then echo "CLI tests: all passed"; else echo "CLI tests: FAILURES"; fi
exit $fail

# =====================================================================
# EXPLANATION (in simple words)
# ---------------------------------------------------------------------
# This shell script tests refcrypt.exe the way a real user would.
#
#  - check():       runs a command and prints "ok" if it succeeds.
#  - check_fails(): prints "ok" if the command FAILS (for things that
#                   must be refused, such as a wrong key).
#
# What it tests:
#  1. keygen makes a 32-byte key file.
#  2. An empty file, a small text file and 200 KB of random data all
#     encrypt and decrypt back to exactly the same bytes (cmp), and each
#     encrypted file is exactly 48 bytes bigger.
#  3. Encrypting the same file twice gives different results (random nonce).
#  4. Decrypting with the wrong key fails.
#  5. Changing one byte of an encrypted file makes decryption fail, and
#     no output file is created.
#  6. "trace" writes a CSV with 1 header line + 49 points.
# Temporary files go in tests/tmp and are deleted at the end.
# Run it with:  mingw32-make check-cli
# =====================================================================
