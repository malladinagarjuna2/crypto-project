/* HMAC-SHA256 per RFC 2104 / FIPS 198-1. */
#include <string.h>
#include "hmac.h"

void hmac_sha256_init(hmac_sha256_ctx *ctx, const void *key, size_t key_len)
{
    uint8_t k[SHA256_BLOCK_SIZE], pad[SHA256_BLOCK_SIZE];
    int i;

    memset(k, 0, sizeof k);
    if (key_len > SHA256_BLOCK_SIZE)
        sha256(key, key_len, k);
    else if (key_len > 0)
        memcpy(k, key, key_len);

    for (i = 0; i < SHA256_BLOCK_SIZE; i++) pad[i] = k[i] ^ 0x36;
    sha256_init(&ctx->inner);
    sha256_update(&ctx->inner, pad, sizeof pad);

    for (i = 0; i < SHA256_BLOCK_SIZE; i++) pad[i] = k[i] ^ 0x5c;
    sha256_init(&ctx->outer);
    sha256_update(&ctx->outer, pad, sizeof pad);

    memset(k, 0, sizeof k);
    memset(pad, 0, sizeof pad);
}

void hmac_sha256_update(hmac_sha256_ctx *ctx, const void *data, size_t len)
{
    sha256_update(&ctx->inner, data, len);
}

void hmac_sha256_final(hmac_sha256_ctx *ctx, uint8_t out[SHA256_DIGEST_SIZE])
{
    uint8_t ih[SHA256_DIGEST_SIZE];

    sha256_final(&ctx->inner, ih);
    sha256_update(&ctx->outer, ih, sizeof ih);
    sha256_final(&ctx->outer, out);
    memset(ih, 0, sizeof ih);
}

void hmac_sha256(const void *key, size_t key_len, const void *data, size_t len,
                 uint8_t out[SHA256_DIGEST_SIZE])
{
    hmac_sha256_ctx ctx;
    hmac_sha256_init(&ctx, key, key_len);
    hmac_sha256_update(&ctx, data, len);
    hmac_sha256_final(&ctx, out);
}

int ct_equal(const void *a, const void *b, size_t len)
{
    const volatile uint8_t *x = (const volatile uint8_t *)a;
    const volatile uint8_t *y = (const volatile uint8_t *)b;
    uint8_t diff = 0;
    size_t i;

    for (i = 0; i < len; i++)
        diff |= x[i] ^ y[i];
    return diff == 0;
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * HMAC(key, msg) = SHA256( (key XOR opad) + SHA256( (key XOR ipad) + msg ) )
 *
 *  - hmac_sha256_init():
 *      1. If the key is longer than 64 bytes, hash it down to 32 bytes.
 *         Shorter keys are padded with zeros up to 64 bytes.
 *      2. XOR the key with 0x36 (the "inner pad") and start the inner hash.
 *      3. XOR the key with 0x5c (the "outer pad") and start the outer hash.
 *      4. Wipe the temporary copies of the key from memory.
 *  - hmac_sha256_update(): the message goes into the inner hash.
 *  - hmac_sha256_final(): finish the inner hash, feed its 32-byte result
 *    into the outer hash, and finish that. The result is the HMAC tag.
 *  - hmac_sha256(): all three steps in one call.
 *  - ct_equal(): ORs together the XOR of every byte pair. The result is
 *    0 only if every byte matched. "volatile" stops the compiler from
 *    "optimizing" it into an early-exit loop.
 *
 * Checked against RFC 4231 test vectors in tests/test_main.c.
 * ===================================================================== */
