#ifndef REFCRYPT_STATS_H
#define REFCRYPT_STATS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    double entropy;          /* Shannon entropy, bits per byte (max 8) */
    double chi2;             /* chi-square over 256 byte values, 255 dof */
    double chi2_p;           /* p-value of chi2 */
    double serial_corr;      /* correlation of byte i with byte i+1 */
    double monobit_p;        /* NIST frequency (monobit) test p-value */
    double runs_p;           /* NIST runs test p-value */
} stats_result;

void stats_compute(const uint8_t *data, size_t len, stats_result *out);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * These are statistical tests that check whether data "looks random".
 * Good ciphertext should be indistinguishable from random noise.
 *
 *  - entropy     : how unpredictable each byte is. 8.0 = perfectly random.
 *  - chi2/chi2_p : are all 256 byte values equally common? p-value
 *                  between 0.01 and 0.99 = normal for random data.
 *  - serial_corr : does one byte predict the next one? Should be ~0.
 *  - monobit_p   : are there about as many 1-bits as 0-bits?
 *  - runs_p      : do 1s and 0s switch as often as in random data?
 * For the p-values, a value below 0.01 means "suspiciously non-random".
 * ===================================================================== */
