#include "rng.h"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>

int rng_bytes(uint8_t *buf, size_t len)
{
    HCRYPTPROV prov;
    BOOL ok;

    if (len == 0) return 0;
    if (!CryptAcquireContextA(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
        return -1;
    ok = CryptGenRandom(prov, (DWORD)len, buf);
    CryptReleaseContext(prov, 0);
    return ok ? 0 : -1;
}

#else
#include <stdio.h>

int rng_bytes(uint8_t *buf, size_t len)
{
    FILE *f = fopen("/dev/urandom", "rb");
    size_t got;
    if (!f) return -1;
    got = fread(buf, 1, len, f);
    fclose(f);
    return got == len ? 0 : -1;
}
#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * On Windows (_WIN32) we use the built-in CryptGenRandom service:
 *   1. CryptAcquireContextA opens a connection to the Windows crypto
 *      provider. CRYPT_VERIFYCONTEXT means "no stored keys needed, just
 *      random numbers".
 *   2. CryptGenRandom fills the buffer with secure random bytes.
 *   3. CryptReleaseContext closes the connection.
 * On Linux/macOS we read the special file /dev/urandom, which the
 * operating system fills with secure randomness.
 * The function returns 0 if it worked and -1 if it didn't.
 * ===================================================================== */
