#include <string.h>
#include "grid.h"

void grid_build(rc_key *k, const uint8_t grid_seed[32])
{
    const uint32_t span = (uint32_t)(GRID_N_MAX - GRID_N_MIN + 1);   /* 92866 */
    kdf_stream rng;
    chaos_state cs;
    uint8_t chaos_seed[16];
    int x, y, z;

    kdf_stream_init(&rng, grid_seed, "grid");
    kdf_stream_read(&rng, chaos_seed, sizeof chaos_seed);
    chaos_init(&cs, chaos_seed);

    for (z = 0; z < RC_GRID; z++)
        for (y = 0; y < RC_GRID; y++)
            for (x = 0; x < RC_GRID; x++) {
                uint32_t u = chaos_sample(&cs) ^ kdf_stream_u32(&rng);
                k->grid[z][y][x] = GRID_N_MIN + (fx_t)(u % span);
            }

    memset(chaos_seed, 0, sizeof chaos_seed);
    memset(&cs, 0, sizeof cs);
    kdf_stream_wipe(&rng);
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * grid_build() grows the secret crystal:
 *
 *  1. Opens a random byte stream from the grid seed, labelled "grid".
 *  2. Reads 16 bytes from it to choose the chaotic system's start point.
 *  3. For every cell (z, y, x):
 *       - takes 32 bits from the chaotic system (the "shape"),
 *       - XORs them with 32 bits from the HMAC stream (the "security"),
 *       - maps the result into the range 65536..158401 (n = 1.0..2.417)
 *         with "% span".
 *  4. Wipes the temporary secrets from memory.
 *
 * Why both? The chaos gives the structure the original design wanted,
 * but chaotic math with integers eventually repeats itself. XOR-ing in
 * HMAC output means the crystal stays unpredictable even if the chaos
 * turns out to be weak.
 * ===================================================================== */
