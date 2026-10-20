#ifndef REFCRYPT_KEYSCHED_H
#define REFCRYPT_KEYSCHED_H

#include <stddef.h>
#include <stdint.h>
#include "fixed.h"

#define RC_KEY_BYTES 32
#define RC_GRID      16                 /* crystal is RC_GRID^3 cells */
#define RC_ROUNDS    16

typedef struct {
    fx_t     grid[RC_GRID][RC_GRID][RC_GRID];   /* refractive index n[z][y][x], Q16.16 */
    uint8_t  sbox[256];
    uint64_t round_key[RC_ROUNDS];
    uint8_t  mac_key[32];
} rc_key;

/* Expands a 256-bit master key into every subkey. */
void rc_key_init(rc_key *k, const uint8_t master[RC_KEY_BYTES]);
void rc_key_wipe(rc_key *k);

/* Deterministic pseudo-random byte stream:
 * block_i = HMAC-SHA256(seed, label || be32(i)), concatenated. */
typedef struct {
    uint8_t  seed[32];
    char     label[16];
    uint32_t counter;
    uint8_t  block[32];
    size_t   used;
} kdf_stream;

void     kdf_stream_init(kdf_stream *s, const uint8_t seed[32], const char *label);
void     kdf_stream_read(kdf_stream *s, uint8_t *out, size_t len);
uint32_t kdf_stream_u32(kdf_stream *s);
void     kdf_stream_wipe(kdf_stream *s);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * The "key schedule" turns your one 32-byte secret key into all the
 * secret pieces the cipher needs. They are stored together in rc_key:
 *
 *  - grid      : the 16 x 16 x 16 "crystal". Each cell holds a refractive
 *                index n between 1.0 and 2.417, stored in Q16.16 form.
 *  - sbox      : a secret shuffle of the numbers 0..255, used to scramble
 *                values as the ray passes through cells.
 *  - round_key : 16 secret 64-bit numbers, one per Feistel round.
 *  - mac_key   : a separate 32-byte key for the HMAC tamper check.
 *
 * rc_key_init() fills all of these in from the master key.
 * rc_key_wipe() erases them from memory when you are done.
 *
 * kdf_stream is a "random-looking byte tap". From a 32-byte seed and a
 * label it produces as many bytes as you want, by computing
 * HMAC(seed, label + 0), HMAC(seed, label + 1), ... one after another.
 * We need this because plain HKDF can only make 8160 bytes, and the
 * crystal alone needs 4096 cells * 4 bytes = 16384 bytes.
 * ===================================================================== */
