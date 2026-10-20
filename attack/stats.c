#include <math.h>
#include <string.h>
#include "stats.h"

/* Upper-tail p-value of chi-square with k dof (Wilson-Hilferty approximation). */
static double chi2_pvalue(double x, double k)
{
    double z = (pow(x / k, 1.0 / 3.0) - (1.0 - 2.0 / (9.0 * k))) / sqrt(2.0 / (9.0 * k));
    return 0.5 * erfc(z / sqrt(2.0));
}

static int popcount8(uint8_t v)
{
    int c = 0;
    while (v) { c += v & 1; v >>= 1; }
    return c;
}

void stats_compute(const uint8_t *data, size_t len, stats_result *out)
{
    double counts[256], expected = (double)len / 256.0;
    double n = (double)len, sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
    double ones = 0, bits = n * 8.0, runs = 1, pi;
    int prev_bit = -1;
    size_t i;
    int b;

    memset(out, 0, sizeof *out);
    if (len < 2) return;
    memset(counts, 0, sizeof counts);

    for (i = 0; i < len; i++) {
        counts[data[i]] += 1;
        ones += popcount8(data[i]);
        for (b = 7; b >= 0; b--) {
            int bit = (data[i] >> b) & 1;
            if (prev_bit >= 0 && bit != prev_bit) runs++;
            prev_bit = bit;
        }
        if (i + 1 < len) {
            double x = data[i], y = data[i + 1];
            sx += x; sy += y; sxx += x * x; syy += y * y; sxy += x * y;
        }
    }

    for (b = 0; b < 256; b++) {
        if (counts[b] > 0) {
            double p = counts[b] / n;
            out->entropy -= p * log2(p);
        }
        out->chi2 += (counts[b] - expected) * (counts[b] - expected) / expected;
    }
    out->chi2_p = chi2_pvalue(out->chi2, 255.0);

    {
        double m = n - 1;
        double cov = sxy / m - (sx / m) * (sy / m);
        double vx = sxx / m - (sx / m) * (sx / m), vy = syy / m - (sy / m) * (sy / m);
        out->serial_corr = (vx > 0 && vy > 0) ? cov / sqrt(vx * vy) : 1.0;
    }

    /* NIST SP 800-22 frequency (monobit) test */
    out->monobit_p = erfc(fabs(2 * ones - bits) / sqrt(bits) / sqrt(2.0));

    /* NIST SP 800-22 runs test */
    pi = ones / bits;
    if (fabs(pi - 0.5) >= 2.0 / sqrt(bits))
        out->runs_p = 0.0;
    else
        out->runs_p = erfc(fabs(runs - 2.0 * bits * pi * (1 - pi)) /
                           (2.0 * sqrt(2.0 * bits) * pi * (1 - pi)));
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * stats_compute() reads the data once and calculates every test:
 *
 *  - Counting: how often each byte value 0..255 appears, how many 1-bits
 *    there are, and how many "runs" (blocks of identical bits) there are.
 *  - Shannon entropy: H = - sum p(x) * log2 p(x). If all 256 values are
 *    equally common, H = 8 (the maximum).
 *  - Chi-square: sum of (observed - expected)^2 / expected. Random data
 *    gives about 255. chi2_pvalue() turns that into a probability using
 *    the Wilson-Hilferty formula (a well-known approximation).
 *  - Serial correlation: the standard "Pearson correlation" between each
 *    byte and the next one. 0 = no relationship.
 *  - Monobit (NIST SP 800-22): if the 1s and 0s are unbalanced, the
 *    p-value becomes tiny.
 *  - Runs (NIST SP 800-22): compares the number of bit switches with
 *    what random data would give.
 * erfc() is the "complementary error function" from math.h. It turns a
 * z-score into a probability.
 * ===================================================================== */
