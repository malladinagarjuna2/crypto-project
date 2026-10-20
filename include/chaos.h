#ifndef REFCRYPT_CHAOS_H
#define REFCRYPT_CHAOS_H

#include <stdint.h>
#include "fixed.h"

/* 4D hyperchaotic Lorenz-type system, integrated with fixed-point RK4:
 *   x' = a(y - x) + w
 *   y' = c x - y - x z
 *   z' = x y - b z
 *   w' = -y z + r w
 */
typedef struct {
    fx_t v[4];          /* x, y, z, w */
} chaos_state;

#define CHAOS_WARMUP 1000

/* seed: 16 bytes -> initial conditions, then CHAOS_WARMUP steps discarded. */
void     chaos_init(chaos_state *s, const uint8_t seed[16]);
void     chaos_step(chaos_state *s);
uint32_t chaos_sample(chaos_state *s);   /* steps once and returns 32 mixed bits */

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * A "chaotic system" is a set of equations where tiny changes in the
 * starting point grow into huge differences later (the butterfly
 * effect). We use a 4-variable version of the famous Lorenz system to
 * give the crystal an interesting, key-dependent structure.
 *
 *  - chaos_state : the current position (x, y, z, w), in Q16.16.
 *  - chaos_init(): turns 16 key-derived bytes into a starting point, then
 *                  runs 1000 steps and throws them away ("warm-up") so the
 *                  system has settled into its chaotic motion.
 *  - chaos_step(): moves the system forward by one small time step.
 *  - chaos_sample(): steps once and squeezes the 4 numbers into one
 *                  32-bit value.
 *
 * Note: security does NOT depend on this chaos (fixed-point chaos
 * eventually repeats). The crystal also mixes in HMAC-based randomness.
 * ===================================================================== */
