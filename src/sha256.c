/* SHA-256 per FIPS 180-4. */
#include <string.h>
#include "sha256.h"

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void compress(uint32_t s[8], const uint8_t blk[64])
{
    uint32_t w[64], a, b, c, d, e, f, g, h;
    int i;

    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)blk[4 * i] << 24) | ((uint32_t)blk[4 * i + 1] << 16) |
               ((uint32_t)blk[4 * i + 2] << 8) | (uint32_t)blk[4 * i + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = s[0]; b = s[1]; c = s[2]; d = s[3]; e = s[4]; f = s[5]; g = s[6]; h = s[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    s[0] += a; s[1] += b; s[2] += c; s[3] += d;
    s[4] += e; s[5] += f; s[6] += g; s[7] += h;
}

void sha256_init(sha256_ctx *ctx)
{
    static const uint32_t iv[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    memcpy(ctx->state, iv, sizeof iv);
    ctx->total_len = 0;
    ctx->buf_len = 0;
}

void sha256_update(sha256_ctx *ctx, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    ctx->total_len += len;
    if (ctx->buf_len > 0) {
        size_t take = SHA256_BLOCK_SIZE - ctx->buf_len;
        if (take > len) take = len;
        memcpy(ctx->buf + ctx->buf_len, p, take);
        ctx->buf_len += take; p += take; len -= take;
        if (ctx->buf_len < SHA256_BLOCK_SIZE)
            return;
        compress(ctx->state, ctx->buf);
        ctx->buf_len = 0;
    }
    while (len >= SHA256_BLOCK_SIZE) {
        compress(ctx->state, p);
        p += SHA256_BLOCK_SIZE; len -= SHA256_BLOCK_SIZE;
    }
    memcpy(ctx->buf, p, len);
    ctx->buf_len = len;
}

void sha256_final(sha256_ctx *ctx, uint8_t out[SHA256_DIGEST_SIZE])
{
    uint64_t bits = ctx->total_len * 8;
    int i;

    ctx->buf[ctx->buf_len++] = 0x80;
    if (ctx->buf_len > 56) {
        memset(ctx->buf + ctx->buf_len, 0, SHA256_BLOCK_SIZE - ctx->buf_len);
        compress(ctx->state, ctx->buf);
        ctx->buf_len = 0;
    }
    memset(ctx->buf + ctx->buf_len, 0, 56 - ctx->buf_len);
    for (i = 0; i < 8; i++)
        ctx->buf[56 + i] = (uint8_t)(bits >> (56 - 8 * i));
    compress(ctx->state, ctx->buf);

    for (i = 0; i < 8; i++) {
        out[4 * i]     = (uint8_t)(ctx->state[i] >> 24);
        out[4 * i + 1] = (uint8_t)(ctx->state[i] >> 16);
        out[4 * i + 2] = (uint8_t)(ctx->state[i] >> 8);
        out[4 * i + 3] = (uint8_t)(ctx->state[i]);
    }
    memset(ctx, 0, sizeof *ctx);
}

void sha256(const void *data, size_t len, uint8_t out[SHA256_DIGEST_SIZE])
{
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * SHA-256 is a "hash function": it takes any amount of data and squeezes
 * it into a 32-byte fingerprint. Change one bit of the input and the
 * fingerprint changes completely. You cannot go backwards from the
 * fingerprint to the data. We use it to turn the secret key into
 * sub-keys (via HMAC and HKDF).
 *
 * How the code works:
 *  - K[64]: 64 fixed constants defined by the standard (taken from the cube
 *    roots of the first 64 prime numbers). Every SHA-256 uses the same ones.
 *  - ROTR: "rotate right". The bits slide right and the ones that fall off
 *    come back in on the left.
 *  - compress(): the heart of SHA-256. It takes one 64-byte chunk of data
 *    and stirs it into the 8 state words through 64 rounds of rotations,
 *    XORs and additions. Each round mixes the bits further.
 *  - sha256_init(): starts the 8 state words at the standard values.
 *  - sha256_update(): feeds in data of any length. Data is collected in
 *    a 64-byte buffer, and every full 64-byte chunk is compressed.
 *  - sha256_final(): adds "padding" (one 1-bit, then zeros, then the
 *    total message length in bits) so the last chunk is complete,
 *    compresses it, and writes the 8 state words out as 32 bytes. It also
 *    wipes the context so no secret data is left in memory.
 *  - sha256(): a one-call shortcut, init + update + final.
 *
 * tests/test_main.c checks this file against the official FIPS 180-4
 * examples (e.g. "abc" and one million 'a' characters).
 * ===================================================================== */
