#include "feistel.h"
#include "raytrace.h"

uint64_t feistel_f(const rc_key *k, uint64_t x, int round)
{
    return rt_trace(k, x ^ k->round_key[round], NULL);
}

void feistel_encrypt_rounds(const rc_key *k, uint64_t *L, uint64_t *R, int rounds)
{
    uint64_t l = *L, r = *R;
    int i;
    for (i = 0; i < rounds; i++) {
        uint64_t t = l ^ feistel_f(k, r, i);
        l = r;
        r = t;
    }
    *L = l; *R = r;
}

void feistel_decrypt_rounds(const rc_key *k, uint64_t *L, uint64_t *R, int rounds)
{
    uint64_t l = *L, r = *R;
    int i;
    for (i = rounds - 1; i >= 0; i--) {
        uint64_t t = r ^ feistel_f(k, l, i);
        r = l;
        l = t;
    }
    *L = l; *R = r;
}

static uint64_t load_be64(const uint8_t *b)
{
    uint64_t v = 0;
    int i;
    for (i = 0; i < 8; i++) v = (v << 8) | b[i];
    return v;
}

static void store_be64(uint8_t *b, uint64_t v)
{
    int i;
    for (i = 7; i >= 0; i--) { b[i] = (uint8_t)v; v >>= 8; }
}

void feistel_encrypt_block(const rc_key *k, const uint8_t in[RC_BLOCK], uint8_t out[RC_BLOCK])
{
    uint64_t L = load_be64(in), R = load_be64(in + 8);
    feistel_encrypt_rounds(k, &L, &R, RC_ROUNDS);
    store_be64(out, L);
    store_be64(out + 8, R);
}

void feistel_decrypt_block(const rc_key *k, const uint8_t in[RC_BLOCK], uint8_t out[RC_BLOCK])
{
    uint64_t L = load_be64(in), R = load_be64(in + 8);
    feistel_decrypt_rounds(k, &L, &R, RC_ROUNDS);
    store_be64(out, L);
    store_be64(out + 8, R);
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * One Feistel round, with the block split into halves L and R:
 *       new L = R
 *       new R = L XOR F(R)
 * To undo it we only need F going FORWARDS:
 *       old R = new L
 *       old L = new R XOR F(new L)
 * So decryption works perfectly even though the ray tracer itself can't
 * be run backwards (fixed-point rounding would make it drift). That is
 * the main reason we chose a Feistel design.
 *
 *  - feistel_f(): XOR with the round key, then run the ray tracer.
 *  - feistel_encrypt_rounds(): applies the round rule `rounds` times,
 *    using round keys 0, 1, 2, ...
 *  - feistel_decrypt_rounds(): the reverse rule, with round keys in
 *    reverse order (..., 2, 1, 0).
 *  - load_be64() / store_be64(): convert 8 bytes to and from a 64-bit
 *    number (big-endian = first byte is the most significant).
 *  - feistel_encrypt_block() / feistel_decrypt_block(): split 16 bytes
 *    into L and R, run all 16 rounds, and join them back together.
 * ===================================================================== */
