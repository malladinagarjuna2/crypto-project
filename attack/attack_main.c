#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "keysched.h"
#include "mode.h"
#include "rng.h"
#include "stats.h"
#include "sac.h"
#include "impulse.h"
#include "known_plaintext.h"

static int g_failures = 0;

static void verdict(int ok, const char *what)
{
    printf("    [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) g_failures++;
}

static void print_stats(const char *label, const uint8_t *data, size_t len)
{
    stats_result s;
    stats_compute(data, len, &s);
    printf("  %s (%lu bytes)\n", label, (unsigned long)len);
    printf("    entropy %.6f bits/byte | chi2 %.1f (p=%.3f) | serial corr %+.5f | "
           "monobit p=%.3f | runs p=%.3f\n",
           s.entropy, s.chi2, s.chi2_p, s.serial_corr, s.monobit_p, s.runs_p);
}

int main(int argc, char **argv)
{
    static rc_key k;
    uint8_t master[32], nonce[RC_NONCE];
    size_t mb = 2, len, i;
    uint8_t *pt, *ct;
    int rounds_list[] = { 1, 2, 3, 4, 8, 16 }, r;
    clock_t t0 = clock();

    for (i = 1; i < (size_t)argc; i++)
        if (strcmp(argv[i], "--mb") == 0 && i + 1 < (size_t)argc)
            mb = (size_t)atoi(argv[++i]);
    if (mb < 1) mb = 1;

    if (rng_bytes(master, sizeof master) != 0) { fprintf(stderr, "RNG failure\n"); return 1; }
    rc_key_init(&k, master);

    printf("=== Refractive cipher: automated cryptanalysis suite ===\n");
    printf("(random key, PASS = the cipher resisted the test)\n\n");

    /* ---------------- 1. Avalanche / differential ---------------- */
    printf("[1] Strict Avalanche Criterion (128x128 matrix, 300 trials per row)\n");
    printf("    rounds | mean flip | worst cell |dev| | bad cells\n");
    for (r = 0; r < (int)(sizeof rounds_list / sizeof rounds_list[0]); r++) {
        sac_result s;
        sac_feistel(&k, rounds_list[r], 300, &s);
        printf("    %6d | %8.2f%% | %15.3f | %8.2f%%\n",
               rounds_list[r], s.mean * 100, s.max_dev, s.frac_bad * 100);
        if (rounds_list[r] == 16) {
            verdict(s.mean > 0.49 && s.mean < 0.51, "16 rounds: mean avalanche within 49-51%");
            verdict(s.max_dev < 0.15, "16 rounds: no SAC matrix cell far from 0.5");
        }
        if (rounds_list[r] == 1)
            verdict(s.frac_bad > 0.4, "1 round is detected as weak (test has teeth)");
    }
    {
        double kf = sac_key(master, 32);
        printf("  key avalanche: flipping 1 key bit changes %.2f%% of ciphertext bits\n", kf * 100);
        verdict(kf > 0.48 && kf < 0.52, "key avalanche within 48-52%");
    }

    /* ---------------- 2. Impulse response / linearity ---------------- */
    printf("\n[2] Impulse-response linearity test  E(a^b) == E(a)^E(b)^E(0)\n");
    {
        double l1 = impulse_linearity(&k, 1, 2000, 1);
        double l16s = impulse_linearity(&k, 16, 2000, 1);
        double l16 = impulse_linearity(&k, 16, 2000, 0);
        printf("    1 round, sparse impulses : relation holds %.1f%%\n", l1 * 100);
        printf("    16 rounds, sparse        : relation holds %.1f%%\n", l16s * 100);
        printf("    16 rounds, random pairs  : relation holds %.1f%%\n", l16 * 100);
        verdict(l1 > 0.99, "1-round cipher is caught as linear (test has teeth)");
        verdict(l16s == 0 && l16 == 0, "16 rounds: no linear transfer function");
    }

    /* ---------------- 3. Codebook (known-plaintext) attack ---------------- */
    printf("\n[3] Chosen-plaintext codebook attack\n");
    {
        static const char secret[] =
            "Meet at the lab at 9pm. The vault code is 4471-2290. Bring the crystal.";
        size_t n = sizeof secret - 1;
        double weak = kpa_attack_weak(&k, (const uint8_t *)secret, n);
        double real = kpa_attack_real(&k, (const uint8_t *)secret, n);
        printf("    original plan (byte -> ray, no nonce): %.1f%% of secret recovered\n", weak * 100);
        printf("    final cipher (Feistel + CTR + nonce):  %.1f%% of secret recovered\n", real * 100);
        verdict(weak > 0.99, "original byte-per-ray design is broken by the codebook (as predicted)");
        verdict(real < 0.10, "final cipher resists the codebook attack");
    }

    /* ---------------- 4. Statistics on bulk ciphertext ---------------- */
    len = mb * 1000000;
    printf("\n[4] Statistical tests on %lu MB of ciphertext (this is the slow part)\n", (unsigned long)mb);
    pt = (uint8_t *)malloc(len);
    ct = (uint8_t *)malloc(len + RC_OVERHEAD);
    if (!pt || !ct) { fprintf(stderr, "out of memory\n"); return 1; }
    for (i = 0; i < len; i++)                                 /* very low-entropy "text" */
        pt[i] = (uint8_t)("The quick brown fox jumps over the lazy dog. "[i % 45]);
    print_stats("plaintext (repeating English sentence)", pt, len);
    rng_bytes(nonce, RC_NONCE);
    rc_seal(&k, nonce, pt, len, ct);
    {
        stats_result s;
        const uint8_t *body = ct + RC_MAGIC_LEN + RC_NONCE;
        print_stats("ciphertext", body, len);
        stats_compute(body, len, &s);
        verdict(s.entropy > 7.999, "entropy > 7.999 bits/byte");
        verdict(s.chi2_p > 0.001 && s.chi2_p < 0.999, "chi-square p-value not extreme");
        verdict(s.serial_corr > -0.005 && s.serial_corr < 0.005, "no byte-to-byte correlation");
        verdict(s.monobit_p > 0.001, "NIST monobit test");
        verdict(s.runs_p > 0.001, "NIST runs test");
    }
    free(pt); free(ct);
    rc_key_wipe(&k);

    printf("\nfinished in %.1f s: %s\n", (double)(clock() - t0) / CLOCKS_PER_SEC,
           g_failures ? "SOME CHECKS FAILED" : "all checks passed");
    printf("Reminder: passing these tests shows no OBVIOUS flaw; it does not prove security.\n");
    return g_failures ? 1 : 0;
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * attack.exe runs every attack against a freshly generated random key
 * and prints PASS/FAIL for each property. Options: --mb N chooses how
 * many megabytes to encrypt for the statistics (default 2; the original
 * plan's 10 MB takes about a minute).
 *
 *  [1] Avalanche: measures the SAC matrix after 1, 2, 3, 4, 8 and 16
 *      rounds. You can watch the mixing improve round by round. 16 rounds
 *      must be close to 50% everywhere. 1 round MUST look bad, which
 *      proves the test is able to detect weakness. Also checks that one
 *      flipped key bit changes about half the ciphertext.
 *  [2] Impulse/linearity: checks that the cipher can't be predicted from
 *      single-bit "impulse" inputs. Again, 1 round must be caught.
 *  [3] Codebook attack: breaks the original plan's design (~100% of a
 *      secret sentence recovered) and fails on the final design (~0%).
 *  [4] Statistics: encrypts a boring repeated sentence and checks that
 *      the ciphertext looks like random noise (entropy, chi-square,
 *      correlation, NIST monobit and runs tests).
 *
 * verdict() prints the result and counts failures. The program's exit
 * code is 0 only if everything passed.
 * ===================================================================== */
