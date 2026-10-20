#ifndef REFCRYPT_GRID_H
#define REFCRYPT_GRID_H

#include "keysched.h"
#include "chaos.h"

#define GRID_N_MIN FX_ONE              /* 1.000 */
#define GRID_N_MAX FX_CONST(2.417)     /* 2.417 (diamond) = 158401 */

/* Fills k->grid from a 32-byte grid seed. */
void grid_build(rc_key *k, const uint8_t grid_seed[32]);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * The "grid" is the secret 3D crystal: 16 x 16 x 16 = 4096 small cubes,
 * each with its own refractive index n (how strongly it bends light).
 *
 *  - GRID_N_MIN / GRID_N_MAX: the allowed range of n, from 1.0 (like air)
 *    to 2.417 (like diamond), stored in Q16.16 (65536 to 158401).
 *  - grid_build(): fills every cell using the key-derived seed.
 * ===================================================================== */
