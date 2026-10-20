#include "raytrace.h"

#define CELL(c) ((fx_t)(c) * FX_ONE)

uint64_t rt_mix64(uint64_t z)
{
    z ^= z >> 30; z *= 0xbf58476d1ce4e5b9ULL;
    z ^= z >> 27; z *= 0x94d049bb133111ebULL;
    z ^= z >> 31;
    return z;
}

void rt_normalize(fx_t d[3])
{
    /* |d|^2 in Q32.32, sqrt of that is |d| in Q16.16 */
    uint64_t len2 = (uint64_t)((int64_t)d[0] * d[0]) + (uint64_t)((int64_t)d[1] * d[1]) +
                    (uint64_t)((int64_t)d[2] * d[2]);
    fx_t len = (fx_t)fx_isqrt64(len2);
    int i;

    if (len == 0) {               /* degenerate: should not happen, but stay defined */
        d[0] = 0; d[1] = 0; d[2] = FX_ONE;
        return;
    }
    for (i = 0; i < 3; i++)
        d[i] = fx_div(d[i], len);
}

int rt_boundary(fx_t d[3], int axis, fx_t n1, fx_t n2)
{
    fx_t sign = d[axis] < 0 ? -1 : 1;
    fx_t c1 = fx_abs(d[axis]);                       /* cos(theta1) = -N.I */
    fx_t r  = fx_div(n1, n2);
    fx_t k  = FX_ONE - fx_mul(fx_mul(r, r), FX_ONE - fx_mul(c1, c1));
    fx_t c2 = fx_sqrt(k);                            /* 0 when k < 0 */
    int32_t tir  = (int32_t)(k < 0);
    int32_t mask = -tir;                             /* all ones if TIR */
    int i;

    /* compute both outcomes, then select without branching on secret data */
    for (i = 0; i < 3; i++) {
        fx_t refr = (i == axis) ? sign * c2 : fx_mul(r, d[i]);   /* R = rI + (rc1 - c2)N */
        fx_t refl = (i == axis) ? -d[i] : d[i];                 /* R = I + 2c1N        */
        d[i] = (fx_t)(((uint32_t)refr & ~(uint32_t)mask) | ((uint32_t)refl & (uint32_t)mask));
    }
    rt_normalize(d);
    return tir ? RT_TIR : RT_REFRACT;
}

static void record(rt_path *path, const fx_t p[3], const fx_t d[3], int event)
{
    rt_point *pt;
    if (!path || path->count > RT_STEPS) return;
    pt = &path->pts[path->count++];
    pt->p[0] = p[0]; pt->p[1] = p[1]; pt->p[2] = p[2];
    pt->d[0] = d[0]; pt->d[1] = d[1]; pt->d[2] = d[2];
    pt->event = (uint8_t)event;
    if (event == RT_REFRACT) path->refract++;
    else if (event == RT_TIR) path->tir++;
    else if (event == RT_WALL) path->wall++;
}

uint64_t rt_trace(const rc_key *k, uint64_t in, rt_path *path)
{
    const fx_t size = CELL(RC_GRID);
    fx_t p[3], d[3];
    int c[3], step, i;
    uint64_t acc = 0x6a09e667f3bcc908ULL ^ in;
    int32_t ax, ay;
    int64_t dz2;

    if (path) { path->count = 0; path->refract = path->tir = path->wall = 0; }

    /* ---- entry: 20 bits x, 20 bits y on the z = 0 face, 12+12 bits angle ---- */
    p[0] = (fx_t)(((in & 0xFFFFF) * (uint64_t)size) >> 20);
    p[1] = (fx_t)((((in >> 20) & 0xFFFFF) * (uint64_t)size) >> 20);
    p[2] = 0;
    ax = ((int32_t)((in >> 40) & 0xFFF) - 2048) * 22;           /* |dx| <= 0.6875 */
    ay = ((int32_t)((in >> 52) & 0xFFF) - 2048) * 22;
    d[0] = ax; d[1] = ay;
    dz2 = (int64_t)FX_ONE * FX_ONE - (int64_t)ax * ax - (int64_t)ay * ay;
    d[2] = (fx_t)fx_isqrt64((uint64_t)dz2);                   /* always > 0.5 */
    rt_normalize(d);
    for (i = 0; i < 3; i++) {
        c[i] = (int)(p[i] >> 16);
        if (c[i] >= RC_GRID) c[i] = RC_GRID - 1;
    }
    record(path, p, d, RT_START);

    for (step = 0; step < RT_STEPS; step++) {
        fx_t t[3], tmin;
        int axis = 0, next, event;

        /* ---- DDA: distance to the next cell face along each axis ---- */
        for (i = 0; i < 3; i++) {
            if (d[i] > 0)      t[i] = fx_div(CELL(c[i] + 1) - p[i], d[i]);
            else if (d[i] < 0) t[i] = fx_div(CELL(c[i]) - p[i], d[i]);
            else               t[i] = INT32_MAX;
        }
        if (t[1] < t[axis]) axis = 1;
        if (t[2] < t[axis]) axis = 2;
        tmin = t[axis] < 0 ? 0 : t[axis];

        /* ---- move to that face; snap the crossing axis exactly ---- */
        for (i = 0; i < 3; i++) {
            if (i == axis) continue;
            p[i] = fx_clamp(p[i] + fx_mul(d[i], tmin), CELL(c[i]), CELL(c[i] + 1));
        }
        p[axis] = d[axis] > 0 ? CELL(c[axis] + 1) : CELL(c[axis]);
        next = c[axis] + (d[axis] > 0 ? 1 : -1);

        /* ---- interact with the face ---- */
        if (next < 0 || next >= RC_GRID) {
            d[axis] = -d[axis];                               /* mirror wall */
            event = RT_WALL;
        } else {
            int nc[3];
            fx_t n1, n2;
            nc[0] = c[0]; nc[1] = c[1]; nc[2] = c[2]; nc[axis] = next;
            n1 = k->grid[c[2]][c[1]][c[0]];
            n2 = k->grid[nc[2]][nc[1]][nc[0]];
            event = rt_boundary(d, axis, n1, n2);
            if (event == RT_REFRACT)
                c[axis] = next;
        }

        /* ---- S-box confusion + accumulate the journey ---- */
        {
            uint32_t idx = ((uint32_t)c[2] << 8) | ((uint32_t)c[1] << 4) | (uint32_t)c[0];
            uint8_t sb = k->sbox[(idx ^ (idx >> 8) ^ (uint32_t)step ^ (uint32_t)acc) & 0xFF];
            acc ^= ((uint64_t)sb << 56) ^ ((uint64_t)idx << 40) ^ ((uint64_t)event << 36) ^
                   ((uint64_t)(uint32_t)p[(axis + 1) % 3] << 4) ^ (uint64_t)step;
            acc = rt_mix64(acc + (uint64_t)(uint32_t)d[(axis + 2) % 3]);
        }
        record(path, p, d, event);
    }

    /* ---- compress the exit state into 64 output bits ---- */
    {
        uint64_t pos = (uint64_t)(uint32_t)p[0] ^ ((uint64_t)(uint32_t)p[1] << 21) ^
                       ((uint64_t)(uint32_t)p[2] << 42);
        uint64_t dir = (uint64_t)(uint32_t)d[0] ^ ((uint64_t)(uint32_t)d[1] << 20) ^
                       ((uint64_t)(uint32_t)d[2] << 40);
        return rt_mix64(acc ^ pos ^ rt_mix64(dir));
    }
}

void rt_path_write_csv(const rt_path *path, FILE *out)
{
    static const char *names[] = { "refract", "tir", "wall", "start" };
    int i;
    fprintf(out, "step,event,x,y,z,dx,dy,dz\n");
    for (i = 0; i < path->count; i++) {
        const rt_point *pt = &path->pts[i];
        fprintf(out, "%d,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", i, names[pt->event],
                pt->p[0] / 65536.0, pt->p[1] / 65536.0, pt->p[2] / 65536.0,
                pt->d[0] / 65536.0, pt->d[1] / 65536.0, pt->d[2] / 65536.0);
    }
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * rt_mix64(): scrambles a 64-bit number so that every input bit affects
 *   about half of the output bits (shifts, XORs and multiplications).
 *
 * rt_normalize(): direction vectors must have length 1. We compute
 *   length = sqrt(dx^2 + dy^2 + dz^2) with integer math and divide each
 *   part by it.
 *
 * rt_boundary(): the physics at a face between two cells.
 *   r  = n1 / n2               (ratio of refractive indices)
 *   c1 = |d[axis]|             (cosine of the hitting angle)
 *   k  = 1 - r^2 (1 - c1^2)
 *   If k >= 0 the ray REFRACTS: the sideways parts are multiplied by r
 *   and the part along the axis becomes sqrt(k) (same direction as
 *   before). If k < 0 the ray cannot get through, so it reflects (TIR):
 *   the axis part flips sign.
 *   Both answers are always computed and one is picked with a bit mask,
 *   so there is no "if" on secret data (this reduces timing leaks).
 *
 * rt_trace(): the full journey.
 *   1. ENTRY: the 64-bit input is cut into pieces: 20 bits choose x and
 *      20 bits choose y on the front face (z = 0), and 12 + 12 bits choose
 *      the tilt of the ray. dz is computed so the direction has length 1.
 *   2. Repeat 48 times:
 *      a. "DDA": work out how far the ray must travel to hit the next
 *         face in x, in y and in z, and take the closest one.
 *      b. Move the ray there, and set the crossing coordinate exactly
 *         on the face (avoids rounding drift).
 *      c. If that face is the outer wall, mirror the ray. Otherwise
 *         apply rt_boundary() with the two cells' n values. Only on
 *         refraction does the ray actually enter the next cell.
 *      d. Look up the secret S-box using the cell number, the step and
 *         the running state, and mix everything (cell, event type,
 *         position, direction) into a 64-bit accumulator.
 *   3. EXIT: mix the accumulator with the final position and direction
 *      to get the 64-bit output.
 *
 * rt_path_write_csv(): prints each recorded point as one line of text,
 *   converting Q16.16 back to normal decimal numbers.
 * ===================================================================== */
