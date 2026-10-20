#ifndef REFCRYPT_SHA256_H
#define REFCRYPT_SHA256_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_BLOCK_SIZE  64
#define SHA256_DIGEST_SIZE 32

typedef struct {
    uint32_t state[8];
    uint64_t total_len;               /* bytes processed */
    uint8_t  buf[SHA256_BLOCK_SIZE];
    size_t   buf_len;
} sha256_ctx;

void sha256_init(sha256_ctx *ctx);
void sha256_update(sha256_ctx *ctx, const void *data, size_t len);
void sha256_final(sha256_ctx *ctx, uint8_t out[SHA256_DIGEST_SIZE]);
void sha256(const void *data, size_t len, uint8_t out[SHA256_DIGEST_SIZE]);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * This header lists the SHA-256 functions (the code is in src/sha256.c).
 *
 *  - sha256_ctx : a "context" that remembers progress while hashing.
 *                 It holds the 8 state words, how many bytes have been
 *                 fed in, and a buffer for an unfinished 64-byte chunk.
 *  - sha256_init / sha256_update / sha256_final : hash data piece by
 *                 piece (useful for big files): start, feed, finish.
 *  - sha256()   : hash a whole buffer in one call.
 *
 * The output (digest) is always 32 bytes. SHA256_BLOCK_SIZE (64) is the
 * chunk size the algorithm works on internally.
 * ===================================================================== */
