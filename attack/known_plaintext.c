#include <stdlib.h>
#include <string.h>
#include "known_plaintext.h"
#include "raytrace.h"
#include "mode.h"
#include "rng.h"

/* The original plan: each plaintext byte picks a ray; the ciphertext is the
 * (digest of the) exit state. No nonce, no rounds, no chaining. */
static uint64_t weak_encrypt_byte(const rc_key *k, uint8_t b)
{
    return rt_trace(k, (uint64_t)b * 0x0101010101010101ULL, NULL);
}

double kpa_attack_weak(const rc_key *k, const uint8_t *secret, size_t len)
{
    uint64_t book[256];
    size_t i, hit = 0;
    int b;

    /* attacker phase 1: chosen plaintexts 0..255 -> codebook */
    for (b = 0; b < 256; b++)
        book[b] = weak_encrypt_byte(k, (uint8_t)b);

    /* victim encrypts a secret; attacker phase 2: look up each ciphertext */
    for (i = 0; i < len; i++) {
        uint64_t c = weak_encrypt_byte(k, secret[i]);
        int guess = -1;
        for (b = 0; b < 256; b++)
            if (book[b] == c) { guess = b; break; }
        if (guess == secret[i]) hit++;
    }
    return len ? (double)hit / len : 0;
}

double kpa_attack_real(const rc_key *k, const uint8_t *secret, size_t len)
{
    uint8_t chosen[256], nonce[RC_NONCE];
    uint8_t *sealed_chosen = (uint8_t *)malloc(256 + RC_OVERHEAD);
    uint8_t *sealed_secret = (uint8_t *)malloc(len + RC_OVERHEAD);
    uint8_t book_ct_to_pt[256];
    size_t i, hit = 0;
    int b;

    if (!sealed_chosen || !sealed_secret) { free(sealed_chosen); free(sealed_secret); return -1; }

    /* attacker phase 1: gets all 256 byte values encrypted */
    for (b = 0; b < 256; b++) chosen[b] = (uint8_t)b;
    rng_bytes(nonce, RC_NONCE);
    rc_seal(k, nonce, chosen, 256, sealed_chosen);
    memset(book_ct_to_pt, 0, sizeof book_ct_to_pt);
    for (b = 0; b < 256; b++)
        book_ct_to_pt[sealed_chosen[RC_MAGIC_LEN + RC_NONCE + b]] = (uint8_t)b;

    /* victim encrypts the secret (fresh nonce, as the real cipher always does) */
    rng_bytes(nonce, RC_NONCE);
    rc_seal(k, nonce, secret, len, sealed_secret);

    /* attacker phase 2: apply the codebook byte by byte */
    for (i = 0; i < len; i++)
        if (book_ct_to_pt[sealed_secret[RC_MAGIC_LEN + RC_NONCE + i]] == secret[i]) hit++;

    free(sealed_chosen); free(sealed_secret);
    return len ? (double)hit / len : 0;
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * weak_encrypt_byte(): rebuilds the ORIGINAL plan's cipher. A byte
 *   chooses where the ray enters, and the "ciphertext" is where it
 *   comes out. For a fixed key, the same byte ALWAYS gives the same
 *   output.
 *
 * kpa_attack_weak():
 *   1. The attacker encrypts all 256 possible bytes and stores the
 *      results in a table ("codebook").
 *   2. The victim encrypts a secret message with the same key.
 *   3. For every ciphertext value the attacker finds the matching table
 *      entry and so knows the plaintext byte. No key needed!
 *
 * kpa_attack_real():
 *   The attacker tries exactly the same trick on the real cipher: get
 *   all 256 bytes encrypted, build a table of ciphertext byte ->
 *   plaintext byte, and apply it to the secret. But every message uses
 *   a fresh random nonce, and every position gets a different keystream
 *   byte, so the table is useless. The success rate is about 1/256
 *   (0.4%), the same as random guessing.
 * ===================================================================== */
