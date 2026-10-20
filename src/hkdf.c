#include <string.h>
#include "hkdf.h"

void hkdf_extract(const void *salt, size_t salt_len, const void *ikm, size_t ikm_len,
                  uint8_t prk[SHA256_DIGEST_SIZE])
{
    static const uint8_t zeros[SHA256_DIGEST_SIZE] = { 0 };

    if (salt == NULL || salt_len == 0) {
        salt = zeros;
        salt_len = sizeof zeros;
    }
    hmac_sha256(salt, salt_len, ikm, ikm_len, prk);
}

int hkdf_expand(const uint8_t prk[SHA256_DIGEST_SIZE], const void *info, size_t info_len,
                uint8_t *okm, size_t okm_len)
{
    uint8_t t[SHA256_DIGEST_SIZE];
    size_t t_len = 0, done = 0;
    uint8_t counter = 1;

    if (okm_len > 255 * SHA256_DIGEST_SIZE)
        return -1;

    while (done < okm_len) {
        hmac_sha256_ctx ctx;
        size_t take;

        hmac_sha256_init(&ctx, prk, SHA256_DIGEST_SIZE);
        hmac_sha256_update(&ctx, t, t_len);
        if (info_len > 0)
            hmac_sha256_update(&ctx, info, info_len);
        hmac_sha256_update(&ctx, &counter, 1);
        hmac_sha256_final(&ctx, t);
        t_len = SHA256_DIGEST_SIZE;

        take = okm_len - done;
        if (take > SHA256_DIGEST_SIZE) take = SHA256_DIGEST_SIZE;
        memcpy(okm + done, t, take);
        done += take;
        counter++;
    }
    memset(t, 0, sizeof t);
    return 0;
}

int hkdf(const void *salt, size_t salt_len, const void *ikm, size_t ikm_len,
         const void *info, size_t info_len, uint8_t *okm, size_t okm_len)
{
    uint8_t prk[SHA256_DIGEST_SIZE];
    int rc;

    hkdf_extract(salt, salt_len, ikm, ikm_len, prk);
    rc = hkdf_expand(prk, info, info_len, okm, okm_len);
    memset(prk, 0, sizeof prk);
    return rc;
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * HKDF (RFC 5869) makes many independent keys from one master key.
 *
 *  - hkdf_extract(): PRK = HMAC(salt, input_key). With no salt, it uses
 *    32 zero bytes as the standard says. The PRK is a strong, evenly
 *    random 32-byte key.
 *  - hkdf_expand(): builds the output in 32-byte pieces:
 *        T(1) = HMAC(PRK, info + 0x01)
 *        T(2) = HMAC(PRK, T(1) + info + 0x02)
 *        T(3) = HMAC(PRK, T(2) + info + 0x03) ... and so on,
 *    and glues them together until there are enough bytes. The counter is
 *    one byte, so at most 255 pieces (8160 bytes) can be made.
 *  - hkdf(): extract + expand, then wipe the PRK from memory.
 *
 * In this cipher, different "info" labels ("grid", "sbox", "round",
 * "mac") give completely unrelated keys from the same master key.
 * ===================================================================== */
