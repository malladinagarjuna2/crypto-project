#ifndef REFCRYPT_FEISTEL_H
#define REFCRYPT_FEISTEL_H

#include <stdint.h>
#include "keysched.h"

#define RC_BLOCK 16                 /* 128-bit block */

/* Round function: F(x, i) = rt_trace(crystal, x XOR round_key[i]) */
uint64_t feistel_f(const rc_key *k, uint64_t x, int round);

/* Reduced-round variants exist so the attack suite can study diffusion. */
void feistel_encrypt_rounds(const rc_key *k, uint64_t *L, uint64_t *R, int rounds);
void feistel_decrypt_rounds(const rc_key *k, uint64_t *L, uint64_t *R, int rounds);

/* Full 16-round block cipher on 16 bytes (big-endian halves). */
void feistel_encrypt_block(const rc_key *k, const uint8_t in[RC_BLOCK], uint8_t out[RC_BLOCK]);
void feistel_decrypt_block(const rc_key *k, const uint8_t in[RC_BLOCK], uint8_t out[RC_BLOCK]);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * The Feistel network is the "frame" of the cipher. It encrypts 16
 * bytes (128 bits) at a time.
 *
 *  - feistel_f(): the round function. It XORs the input with that
 *    round's secret key and shoots the result through the crystal with
 *    the ray tracer.
 *  - feistel_encrypt_rounds() / feistel_decrypt_rounds(): run a chosen
 *    number of rounds on the two 64-bit halves L and R. Fewer rounds are
 *    only for the attack tests ("how many rounds until it looks random?").
 *  - feistel_encrypt_block() / feistel_decrypt_block(): the real thing,
 *    16 rounds on a 16-byte block.
 * ===================================================================== */
