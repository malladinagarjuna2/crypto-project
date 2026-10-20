#ifndef REFCRYPT_RAYTRACE_H
#define REFCRYPT_RAYTRACE_H

#include <stdint.h>
#include <stdio.h>
#include "fixed.h"
#include "keysched.h"

#define RT_STEPS 48                 /* fixed number of boundary events per trace */

enum { RT_REFRACT = 0, RT_TIR = 1, RT_WALL = 2, RT_START = 3 };

typedef struct {
    fx_t    p[3];                   /* position in cell units, Q16.16 */
    fx_t    d[3];                   /* direction after the event */
    uint8_t event;
} rt_point;

typedef struct {
    rt_point pts[RT_STEPS + 1];
    int      count;
    int      refract, tir, wall;
} rt_path;

/* Applies Snell's law / total internal reflection to direction d when it
 * crosses a face perpendicular to `axis` from index n1 into index n2.
 * Returns RT_REFRACT or RT_TIR. d is renormalised to unit length. */
int  rt_boundary(fx_t d[3], int axis, fx_t n1, fx_t n2);
void rt_normalize(fx_t d[3]);

/* The round function core: traces the ray encoded by `in` through the
 * key's crystal and returns a 64-bit digest of the journey.
 * If path != NULL every event is recorded. */
uint64_t rt_trace(const rc_key *k, uint64_t in, rt_path *path);

uint64_t rt_mix64(uint64_t z);

/* Writes a path as CSV: step,event,x,y,z,dx,dy,dz (real numbers). */
void rt_path_write_csv(const rt_path *path, FILE *out);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * This header describes the ray tracer, the "optics engine" of the cipher.
 *
 *  - RT_STEPS (48): every ray makes exactly 48 boundary events. A fixed
 *    count means the time taken does not depend on the secret data.
 *  - Event types: REFRACT (the ray passed into the next cell and bent),
 *    TIR (total internal reflection, the ray bounced back), WALL (the ray
 *    hit the outside of the crystal and mirrored), START (entry point).
 *  - rt_point / rt_path: an optional record of every point the ray
 *    visited, used for plotting and debugging.
 *  - rt_boundary(): the physics. Snell's law in vector form, or TIR.
 *  - rt_normalize(): rescales a direction so its length is exactly 1.
 *  - rt_trace(): turns a 64-bit input into an entry point and angle,
 *    runs the ray through the crystal, and returns a 64-bit scrambled
 *    summary of where it went. This is the "F" of the Feistel cipher.
 *  - rt_mix64(): a fast bit-scrambling function (SplitMix64 finalizer).
 *  - rt_path_write_csv(): saves a recorded path as a spreadsheet-style
 *    text file, which tools/plot_ray.py can draw in 3D.
 * ===================================================================== */
