#include "chaos.h"

/* Parameters a = 10, b = 8/3, c = 28, r = -1; time step h = 0.005 */
#define P_A  FX_CONST(10.0)
#define P_B  FX_CONST(2.6666667)
#define P_C  FX_CONST(28.0)
#define P_R  FX_CONST(-1.0)
#define H    FX_CONST(0.005)
#define LIMIT FX_CONST(1000.0)

static void deriv(const fx_t s[4], fx_t d[4])
{
    fx_t x = s[0], y = s[1], z = s[2], w = s[3];
    d[0] = fx_mul(P_A, y - x) + w;
    d[1] = fx_mul(P_C, x) - y - fx_mul(x, z);
    d[2] = fx_mul(x, y) - fx_mul(P_B, z);
    d[3] = -fx_mul(y, z) + fx_mul(P_R, w);
}

void chaos_step(chaos_state *st)
{
    fx_t k1[4], k2[4], k3[4], k4[4], t[4];
    fx_t half = H / 2;
    int i;

    deriv(st->v, k1);
    for (i = 0; i < 4; i++) t[i] = st->v[i] + fx_mul(half, k1[i]);
    deriv(t, k2);
    for (i = 0; i < 4; i++) t[i] = st->v[i] + fx_mul(half, k2[i]);
    deriv(t, k3);
    for (i = 0; i < 4; i++) t[i] = st->v[i] + fx_mul(H, k3[i]);
    deriv(t, k4);

    for (i = 0; i < 4; i++) {
        int64_t sum = (int64_t)k1[i] + 2 * (int64_t)k2[i] + 2 * (int64_t)k3[i] + k4[i];
        fx_t inc = fx_mul(H, (fx_t)(sum / 6));
        /* keep the orbit bounded so fixed-point never overflows */
        st->v[i] = fx_clamp(st->v[i] + inc, -LIMIT, LIMIT);
    }
}

void chaos_init(chaos_state *st, const uint8_t seed[16])
{
    int i;
    for (i = 0; i < 4; i++) {
        uint32_t u = ((uint32_t)seed[4 * i] << 24) | ((uint32_t)seed[4 * i + 1] << 16) |
                     ((uint32_t)seed[4 * i + 2] << 8) | seed[4 * i + 3];
        /* initial condition in [-20, 20), never exactly the origin */
        st->v[i] = (fx_t)(u % (uint32_t)FX_CONST(40.0)) - FX_CONST(20.0) + 1;
    }
    for (i = 0; i < CHAOS_WARMUP; i++)
        chaos_step(st);
}

uint32_t chaos_sample(chaos_state *st)
{
    uint32_t x, y, z, w;
    chaos_step(st);
    x = (uint32_t)st->v[0]; y = (uint32_t)st->v[1];
    z = (uint32_t)st->v[2]; w = (uint32_t)st->v[3];
    /* low bits carry the most "chaotic" information */
    return x ^ ((y << 11) | (y >> 21)) ^ ((z << 19) | (z >> 13)) ^ ((w << 7) | (w >> 25));
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * This file moves the 4D chaotic system forward in time.
 *
 *  - The #defines are the system's constants (a=10, b=8/3, c=28, r=-1)
 *    and the time step H = 0.005, all converted to Q16.16.
 *  - deriv(): given the current (x, y, z, w), works out how fast each one
 *    is changing, using the four equations from chaos.h.
 *  - chaos_step(): uses "Runge-Kutta 4" (RK4), a standard accurate way to
 *    follow equations through time. It measures the rate of change 4
 *    times (start, two midpoints, end), averages them with weights
 *    1-2-2-1, and moves forward by H. Each value is clamped to +/-1000 so
 *    the integers can never overflow.
 *  - chaos_init(): reads four 32-bit numbers from the seed and turns
 *    them into starting values between -20 and +20, then runs the
 *    warm-up steps.
 *  - chaos_sample(): steps once, then mixes x, y, z, w into one 32-bit
 *    number by rotating their bits by different amounts and XOR-ing them.
 *
 * Everything is integer math, so the same key gives the same numbers on
 * every computer.
 * ===================================================================== */
