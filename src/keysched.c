#include <string.h>
#include "keysched.h"
#include "hkdf.h"
#include "grid.h"
#include "sbox.h"

static const char SALT[] = "refcrypt-v1";

void kdf_stream_init(kdf_stream *s, const uint8_t seed[32], const char *label)
{
    memcpy(s->seed, seed, 32);
    memset(s->label, 0, sizeof s->label);
    strncpy(s->label, label, sizeof s->label - 1);
    s->counter = 0;
    s->used = sizeof s->block;          /* force refill on first read */
}

static void refill(kdf_stream *s)
{
    hmac_sha256_ctx ctx;
    uint8_t ctr[4];

    ctr[0] = (uint8_t)(s->counter >> 24); ctr[1] = (uint8_t)(s->counter >> 16);
    ctr[2] = (uint8_t)(s->counter >> 8);  ctr[3] = (uint8_t)s->counter;
    hmac_sha256_init(&ctx, s->seed, sizeof s->seed);
    hmac_sha256_update(&ctx, s->label, strlen(s->label));
    hmac_sha256_update(&ctx, ctr, 4);
    hmac_sha256_final(&ctx, s->block);
    s->counter++;
    s->used = 0;
}

void kdf_stream_read(kdf_stream *s, uint8_t *out, size_t len)
{
    while (len > 0) {
        size_t take;
        if (s->used == sizeof s->block)
            refill(s);
        take = sizeof s->block - s->used;
        if (take > len) take = len;
        memcpy(out, s->block + s->used, take);
        s->used += take; out += take; len -= take;
    }
}

uint32_t kdf_stream_u32(kdf_stream *s)
{
    uint8_t b[4];
    kdf_stream_read(s, b, 4);
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

void kdf_stream_wipe(kdf_stream *s)
{
    memset(s, 0, sizeof *s);
}

void rc_key_init(rc_key *k, const uint8_t master[RC_KEY_BYTES])
{
    uint8_t prk[32], seed[32], rk[RC_ROUNDS * 8];
    kdf_stream rng;
    int i, j;

    hkdf_extract(SALT, sizeof SALT - 1, master, RC_KEY_BYTES, prk);

    hkdf_expand(prk, "grid", 4, seed, 32);
    grid_build(k, seed);

    hkdf_expand(prk, "sbox", 4, seed, 32);
    kdf_stream_init(&rng, seed, "sbox");
    sbox_build(k->sbox, &rng);
    kdf_stream_wipe(&rng);

    hkdf_expand(prk, "round", 5, rk, sizeof rk);
    for (i = 0; i < RC_ROUNDS; i++) {
        uint64_t v = 0;
        for (j = 0; j < 8; j++)
            v = (v << 8) | rk[8 * i + j];
        k->round_key[i] = v;
    }

    hkdf_expand(prk, "mac", 3, k->mac_key, sizeof k->mac_key);

    memset(prk, 0, sizeof prk);
    memset(seed, 0, sizeof seed);
    memset(rk, 0, sizeof rk);
}

void rc_key_wipe(rc_key *k)
{
    volatile uint8_t *p = (volatile uint8_t *)k;
    size_t i;
    for (i = 0; i < sizeof *k; i++)
        p[i] = 0;
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * Part 1: the kdf_stream (a never-ending, key-based random byte source)
 *  - kdf_stream_init(): remembers the seed and the label, and starts the
 *    counter at 0.
 *  - refill(): makes the next 32 random bytes as
 *    HMAC(seed, label + counter), then adds 1 to the counter.
 *  - kdf_stream_read(): hands out bytes from the current 32-byte block,
 *    and refills it whenever it runs out.
 *  - kdf_stream_u32(): reads 4 bytes and joins them into one 32-bit number.
 *  - kdf_stream_wipe(): erases the stream's secrets.
 *
 * Part 2: rc_key_init(), the full key schedule
 *  1. HKDF-extract the master key with the salt "refcrypt-v1" -> PRK.
 *  2. PRK + label "grid"  -> seed -> build the 3D crystal.
 *  3. PRK + label "sbox"  -> seed -> shuffle the S-box.
 *  4. PRK + label "round" -> 128 bytes -> 16 round keys of 8 bytes each.
 *  5. PRK + label "mac"   -> 32-byte key for the tamper check.
 *  6. Wipe all temporary secrets.
 * Different labels give completely unrelated outputs, so knowing one
 * subkey tells you nothing about the others.
 *
 * rc_key_wipe() zeroes the whole key structure. "volatile" makes sure
 * the compiler really writes the zeros instead of skipping them.
 * ===================================================================== */
