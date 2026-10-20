#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "keysched.h"
#include "mode.h"
#include "raytrace.h"
#include "rng.h"

static rc_key g_key;

static void usage(void)
{
    fprintf(stderr,
        "refcrypt - 3D chaotic refractive cipher (educational)\n\n"
        "  refcrypt keygen <keyfile>                 create a random 256-bit key\n"
        "  refcrypt enc <keyfile> <in> <out>          encrypt a file\n"
        "  refcrypt dec <keyfile> <in> <out>          decrypt (and verify) a file\n"
        "  refcrypt trace <keyfile> <hex64> [out.csv] dump one ray path as CSV\n");
}

static uint8_t *read_file(const char *path, size_t *len, size_t extra)
{
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    long n;

    if (!f) { perror(path); return NULL; }
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fprintf(stderr, "%s: cannot determine size\n", path);
        fclose(f);
        return NULL;
    }
    buf = (uint8_t *)malloc((size_t)n + extra + 1);
    if (!buf) { fprintf(stderr, "out of memory\n"); fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "%s: read error\n", path);
        free(buf); fclose(f);
        return NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

static int write_file(const char *path, const uint8_t *buf, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }
    if (fwrite(buf, 1, len, f) != len) { fprintf(stderr, "%s: write error\n", path); fclose(f); return -1; }
    return fclose(f) == 0 ? 0 : -1;
}

static int load_key(const char *path)
{
    size_t len;
    uint8_t *raw = read_file(path, &len, 0);
    if (!raw) return -1;
    if (len != RC_KEY_BYTES) {
        fprintf(stderr, "%s: key file must be exactly %d bytes (got %lu)\n",
                path, RC_KEY_BYTES, (unsigned long)len);
        free(raw);
        return -1;
    }
    rc_key_init(&g_key, raw);
    memset(raw, 0, len);
    free(raw);
    return 0;
}

static int cmd_keygen(const char *path)
{
    uint8_t key[RC_KEY_BYTES];
    int rc;
    if (rng_bytes(key, sizeof key) != 0) { fprintf(stderr, "RNG failure\n"); return 1; }
    rc = write_file(path, key, sizeof key);
    memset(key, 0, sizeof key);
    if (rc == 0) printf("wrote 256-bit key to %s\n", path);
    return rc == 0 ? 0 : 1;
}

static int cmd_enc(const char *in, const char *out)
{
    size_t len;
    uint8_t nonce[RC_NONCE], *buf, *res;
    int rc;

    if (!(buf = read_file(in, &len, 0))) return 1;
    if (!(res = (uint8_t *)malloc(len + RC_OVERHEAD))) { free(buf); return 1; }
    if (rng_bytes(nonce, sizeof nonce) != 0) {
        fprintf(stderr, "RNG failure\n");
        free(buf); free(res);
        return 1;
    }
    rc = rc_seal(&g_key, nonce, buf, len, res);
    if (rc == RC_OK) rc = write_file(out, res, len + RC_OVERHEAD);
    else fprintf(stderr, "encryption failed (%d)\n", rc);
    free(buf); free(res);
    if (rc == 0) printf("encrypted %lu bytes -> %s\n", (unsigned long)len, out);
    return rc == 0 ? 0 : 1;
}

static int cmd_dec(const char *in, const char *out)
{
    size_t len, pt_len = 0;
    uint8_t *buf, *pt;
    int rc;

    if (!(buf = read_file(in, &len, 0))) return 1;
    pt = (uint8_t *)malloc(len + 1);
    if (!pt) { free(buf); return 1; }
    rc = rc_open(&g_key, buf, len, pt, &pt_len);
    if (rc == RC_ERR_FORMAT) fprintf(stderr, "%s: not a refcrypt file\n", in);
    else if (rc == RC_ERR_AUTH) fprintf(stderr, "%s: AUTHENTICATION FAILED (wrong key or tampered file)\n", in);
    else if (rc == RC_OK) rc = write_file(out, pt, pt_len);
    if (rc == 0) printf("decrypted %lu bytes -> %s\n", (unsigned long)pt_len, out);
    memset(pt, 0, len);
    free(buf); free(pt);
    return rc == 0 ? 0 : 1;
}

static int cmd_trace(const char *hex, const char *csv)
{
    static rt_path path;
    uint64_t in = strtoull(hex, NULL, 16), out;
    FILE *f = stdout;

    out = rt_trace(&g_key, in, &path);
    if (csv && !(f = fopen(csv, "w"))) { perror(csv); return 1; }
    rt_path_write_csv(&path, f);
    if (csv) fclose(f);
    fprintf(stderr, "input 0x%016llx -> output 0x%016llx  (%d refract, %d TIR, %d wall)\n",
            (unsigned long long)in, (unsigned long long)out, path.refract, path.tir, path.wall);
    return 0;
}

int main(int argc, char **argv)
{
    int rc;
    if (argc < 3) { usage(); return 2; }

    if (strcmp(argv[1], "keygen") == 0)
        return cmd_keygen(argv[2]);

    if (load_key(argv[2]) != 0) return 1;
    if (strcmp(argv[1], "enc") == 0 && argc == 5)        rc = cmd_enc(argv[3], argv[4]);
    else if (strcmp(argv[1], "dec") == 0 && argc == 5)   rc = cmd_dec(argv[3], argv[4]);
    else if (strcmp(argv[1], "trace") == 0 && argc >= 4) rc = cmd_trace(argv[3], argc > 4 ? argv[4] : NULL);
    else { usage(); rc = 2; }
    rc_key_wipe(&g_key);
    return rc;
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * This is the command-line program "refcrypt.exe" that people actually run.
 *
 *  - usage(): prints the help text.
 *  - read_file(): reads a whole file into memory (finds its size,
 *    allocates memory, reads it). write_file(): saves memory to a file.
 *  - load_key(): reads the 32-byte key file and runs the key schedule
 *    (builds the crystal, S-box, round keys, MAC key). The raw key is
 *    wiped from memory afterwards.
 *  - cmd_keygen(): makes a new random key with the OS random generator
 *    and saves it. Keep this file secret!
 *  - cmd_enc(): reads the file, makes a fresh random nonce, seals it
 *    (encrypt + tag) and writes the result (original size + 48 bytes).
 *  - cmd_dec(): reads the encrypted file and opens it. If the tag doesn't
 *    match, it prints AUTHENTICATION FAILED and writes nothing.
 *  - cmd_trace(): runs one ray (given as a hex number) through the
 *    crystal and writes every step as CSV, for tools/plot_ray.py.
 *  - main(): looks at the first word (keygen/enc/dec/trace), calls the
 *    right function, and wipes the key at the end.
 *
 * Example:
 *   refcrypt keygen my.key
 *   refcrypt enc my.key notes.txt notes.enc
 *   refcrypt dec my.key notes.enc notes_back.txt
 * ===================================================================== */
