/* Minimal test harness: each CHECK prints on failure; exit code = failures. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fixed.h"
#include "sha256.h"
#include "hmac.h"
#include "hkdf.h"
#include "keysched.h"
#include "chaos.h"
#include "grid.h"
#include "sbox.h"

static int g_fail = 0, g_pass = 0;

#define CHECK(cond, ...) do {                                   \
        if (cond) { g_pass++; }                                 \
        else { g_fail++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
               printf(__VA_ARGS__); printf("\n"); }             \
    } while (0)

static void to_hex(const uint8_t *b, size_t n, char *out)
{
    static const char *h = "0123456789abcdef";
    size_t i;
    for (i = 0; i < n; i++) { out[2 * i] = h[b[i] >> 4]; out[2 * i + 1] = h[b[i] & 15]; }
    out[2 * n] = 0;
}

static size_t from_hex(const char *s, uint8_t *out)
{
    size_t n = strlen(s) / 2, i;
    for (i = 0; i < n; i++) {
        unsigned v;
        sscanf(s + 2 * i, "%2x", &v);
        out[i] = (uint8_t)v;
    }
    return n;
}

static void check_bytes(const char *name, const uint8_t *got, size_t n, const char *want_hex)
{
    char hex[1024];
    to_hex(got, n, hex);
    CHECK(strcmp(hex, want_hex) == 0, "%s\n   got  %s\n   want %s", name, hex, want_hex);
}

/* ---------------- fixed-point ---------------- */
static void test_fixed(void)
{
    CHECK(FX_ONE == 65536, "FX_ONE");
    CHECK(FX_CONST(2.417) == 158401, "FX_CONST(2.417) = %ld", (long)FX_CONST(2.417));
    CHECK(FX_CONST(-0.866025) == -56756, "FX_CONST(-0.866025) = %ld", (long)FX_CONST(-0.866025));
    CHECK(fx_mul(FX_CONST(1.5), FX_CONST(1.5)) == FX_CONST(2.25), "1.5*1.5");
    CHECK(fx_mul(FX_CONST(-1.5), FX_CONST(1.5)) == FX_CONST(-2.25), "-1.5*1.5");
    CHECK(fx_mul(-1, 1) == -1, "floor(-1/65536) must be -1, got %ld", (long)fx_mul(-1, 1));
    CHECK(fx_div(FX_CONST(1.5), FX_CONST(1.0)) == FX_CONST(1.5), "1.5/1");
    CHECK(fx_div(FX_ONE, FX_CONST(1.5)) == 43690, "1/1.5 = %ld", (long)fx_div(FX_ONE, FX_CONST(1.5)));
    CHECK(fx_sqrt(FX_CONST(2.25)) == FX_CONST(1.5), "sqrt(2.25)");
    CHECK(fx_sqrt(FX_CONST(2.0)) == 92681, "sqrt(2) = %ld", (long)fx_sqrt(FX_CONST(2.0)));
    CHECK(fx_sqrt(-5) == 0, "sqrt(neg)");
    CHECK(fx_sqrt(FX_CONST(0.4375)) == 43347, "sqrt(0.4375) = %ld", (long)fx_sqrt(FX_CONST(0.4375)));
    CHECK(fx_isqrt64(UINT64_MAX) == 0xFFFFFFFFu, "isqrt64(max)");
    CHECK(fx_mul(INT32_MAX, INT32_MAX) == INT32_MAX, "saturation");

    /* isqrt exhaustive-ish sanity: r^2 <= v < (r+1)^2 */
    {
        uint64_t v; int ok = 1;
        for (v = 0; v < 200000; v += 7) {
            uint64_t r = fx_isqrt64(v);
            if (!(r * r <= v && (r + 1) * (r + 1) > v)) { ok = 0; break; }
        }
        CHECK(ok, "isqrt64 bounds");
    }
}

/* ---------------- SHA-256 (FIPS 180-4 examples) ---------------- */
static void test_sha256(void)
{
    uint8_t d[32];
    const char *m448 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

    sha256("", 0, d);
    check_bytes("sha256(\"\")", d, 32, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    sha256("abc", 3, d);
    check_bytes("sha256(abc)", d, 32, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    sha256(m448, strlen(m448), d);
    check_bytes("sha256(448-bit)", d, 32, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    {   /* one million 'a', fed in uneven chunks to exercise buffering */
        sha256_ctx c; char buf[997]; size_t left = 1000000;
        memset(buf, 'a', sizeof buf);
        sha256_init(&c);
        while (left) { size_t n = left < sizeof buf ? left : sizeof buf; sha256_update(&c, buf, n); left -= n; }
        sha256_final(&c, d);
        check_bytes("sha256(1M a)", d, 32, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    }
}

/* ---------------- HMAC-SHA256 (RFC 4231) ---------------- */
static void test_hmac(void)
{
    uint8_t key[131], d[32];

    memset(key, 0x0b, 20);
    hmac_sha256(key, 20, "Hi There", 8, d);
    check_bytes("RFC4231 TC1", d, 32, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");

    hmac_sha256("Jefe", 4, "what do ya want for nothing?", 28, d);
    check_bytes("RFC4231 TC2", d, 32, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");

    memset(key, 0xaa, 131);
    hmac_sha256(key, 131, "Test Using Larger Than Block-Size Key - Hash Key First", 54, d);
    check_bytes("RFC4231 TC6", d, 32, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");

    CHECK(ct_equal("abc", "abc", 3) == 1, "ct_equal same");
    CHECK(ct_equal("abc", "abd", 3) == 0, "ct_equal diff");
}

/* ---------------- HKDF (RFC 5869) ---------------- */
static void test_hkdf(void)
{
    uint8_t ikm[22], salt[13], info[10], prk[32], okm[42];
    int i;

    memset(ikm, 0x0b, sizeof ikm);
    for (i = 0; i < 13; i++) salt[i] = (uint8_t)i;
    for (i = 0; i < 10; i++) info[i] = (uint8_t)(0xf0 + i);

    hkdf_extract(salt, 13, ikm, 22, prk);
    check_bytes("RFC5869 TC1 PRK", prk, 32, "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5");
    CHECK(hkdf_expand(prk, info, 10, okm, 42) == 0, "expand rc");
    check_bytes("RFC5869 TC1 OKM", okm, 42,
                "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865");

    hkdf_extract(NULL, 0, ikm, 22, prk);
    check_bytes("RFC5869 TC3 PRK", prk, 32, "19ef24a32c717b167f33a91d6f648bdf96596776afdb6377ac434c1c293ccb04");
    hkdf_expand(prk, NULL, 0, okm, 42);
    check_bytes("RFC5869 TC3 OKM", okm, 42,
                "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8");

    CHECK(hkdf_expand(prk, NULL, 0, okm, 255 * 32 + 1) == -1, "expand length limit");
    (void)from_hex;
}

/* ---------------- Phase 2: key schedule, chaos, crystal, S-box ---------------- */
static rc_key g_k1, g_k2;     /* large structs: keep off the stack */

static void test_kdf_stream(void)
{
    uint8_t seed[32], all[100], parts[100];
    kdf_stream a, b;
    size_t i;

    memset(seed, 7, sizeof seed);
    kdf_stream_init(&a, seed, "test");
    kdf_stream_read(&a, all, sizeof all);
    kdf_stream_init(&b, seed, "test");
    for (i = 0; i < sizeof parts; i += 7)
        kdf_stream_read(&b, parts + i, (sizeof parts - i) < 7 ? sizeof parts - i : 7);
    CHECK(memcmp(all, parts, sizeof all) == 0, "kdf_stream chunked reads must match");

    kdf_stream_init(&b, seed, "other");
    kdf_stream_read(&b, parts, sizeof parts);
    CHECK(memcmp(all, parts, sizeof all) != 0, "different label must give different stream");
}

static void test_chaos(void)
{
    chaos_state s;
    uint8_t seed[16];
    uint32_t prev = 0;
    int i, repeats = 0, bounded = 1;

    for (i = 0; i < 16; i++) seed[i] = (uint8_t)(i * 37 + 1);
    chaos_init(&s, seed);
    for (i = 0; i < 5000; i++) {
        uint32_t v = chaos_sample(&s);
        int j;
        if (v == prev) repeats++;
        prev = v;
        for (j = 0; j < 4; j++)
            if (s.v[j] <= -FX_CONST(1000.0) || s.v[j] >= FX_CONST(1000.0)) bounded = 0;
    }
    CHECK(repeats < 5, "chaos output stuck (%d repeats)", repeats);
    CHECK(bounded, "chaos orbit hit the clamp limit");
}

static int bit_diff(const void *a, const void *b, size_t n)
{
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    int d = 0;
    size_t i;
    for (i = 0; i < n; i++) {
        uint8_t v = x[i] ^ y[i];
        while (v) { d += v & 1; v >>= 1; }
    }
    return d;
}

static void test_key_schedule(void)
{
    uint8_t key[32], inv[256];
    int x, y, z, in_range = 1, diff_cells = 0, low_bits = 0, i, ok;
    const int cells = RC_GRID * RC_GRID * RC_GRID;

    for (i = 0; i < 32; i++) key[i] = (uint8_t)i;
    rc_key_init(&g_k1, key);
    rc_key_init(&g_k2, key);
    CHECK(memcmp(&g_k1, &g_k2, sizeof g_k1) == 0, "same key must give identical schedule");

    for (z = 0; z < RC_GRID; z++) for (y = 0; y < RC_GRID; y++) for (x = 0; x < RC_GRID; x++) {
        fx_t n = g_k1.grid[z][y][x];
        if (n < GRID_N_MIN || n > GRID_N_MAX) in_range = 0;
    }
    CHECK(in_range, "grid value outside [1.0, 2.417]");

    CHECK(sbox_is_permutation(g_k1.sbox), "sbox is not a permutation");
    sbox_invert(g_k1.sbox, inv);
    ok = 1;
    for (i = 0; i < 256; i++) if (inv[g_k1.sbox[i]] != i) ok = 0;
    CHECK(ok, "sbox inverse");

    /* flip ONE key bit */
    key[17] ^= 0x04;
    rc_key_init(&g_k2, key);
    for (z = 0; z < RC_GRID; z++) for (y = 0; y < RC_GRID; y++) for (x = 0; x < RC_GRID; x++) {
        uint16_t a = (uint16_t)g_k1.grid[z][y][x], b = (uint16_t)g_k2.grid[z][y][x];
        if (a != b) diff_cells++;
        low_bits += bit_diff(&a, &b, 2);
    }
    {
        double cell_frac = (double)diff_cells / cells;
        double bit_frac = (double)low_bits / (cells * 16.0);
        double rk_frac = bit_diff(g_k1.round_key, g_k2.round_key, sizeof g_k1.round_key) /
                         (8.0 * sizeof g_k1.round_key);
        printf("  key bit flip: %.2f%% of cells changed, %.2f%% of low-16 grid bits, "
               "%.2f%% of round-key bits\n", cell_frac * 100, bit_frac * 100, rk_frac * 100);
        CHECK(cell_frac > 0.99, "too few cells changed: %.4f", cell_frac);
        CHECK(bit_frac > 0.45 && bit_frac < 0.55, "grid bit avalanche %.4f", bit_frac);
        CHECK(rk_frac > 0.40 && rk_frac < 0.60, "round key avalanche %.4f", rk_frac);
    }
    CHECK(memcmp(g_k1.sbox, g_k2.sbox, 256) != 0, "sbox must change with key");
    CHECK(memcmp(g_k1.mac_key, g_k2.mac_key, 32) != 0, "mac key must change with key");

    rc_key_wipe(&g_k2);
    {
        static const rc_key zero;
        CHECK(memcmp(&g_k2, &zero, sizeof zero) == 0, "rc_key_wipe");
    }
}

int main(void)
{
    test_fixed();
    test_sha256();
    test_hmac();
    test_hkdf();
    test_kdf_stream();
    test_chaos();
    test_key_schedule();
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * This program checks that the building blocks give the right answers.
 *
 *  - CHECK(cond, msg): if cond is true, count a pass. If not, print FAIL
 *    with the file, line and message, and count a failure.
 *  - to_hex(): turns bytes into readable text like "ba7816bf..." so they
 *    can be compared with the official answers.
 *  - check_bytes(): compares a result with the expected hex string.
 *  - test_fixed(): checks the Q16.16 math with known answers, e.g.
 *    1.5*1.5 = 2.25, sqrt(2.25) = 1.5, rounding of negatives, overflow
 *    handling, and that integer sqrt is right for many numbers.
 *  - test_sha256(): official FIPS 180-4 examples ("", "abc", a 56-char
 *    string, one million 'a's fed in odd-sized pieces).
 *  - test_hmac(): official RFC 4231 examples, plus ct_equal().
 *  - test_hkdf(): official RFC 5869 examples, plus the length limit.
 *  - main(): runs everything and prints "N passed, M failed". The exit
 *    code is 0 only if everything passed.
 *
 * If our numbers match the official ones, our implementation is correct.
 * ===================================================================== */
