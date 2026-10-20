#include "fixed.h"

int64_t fx_floor_shr16(int64_t v)
{
    if (v >= 0)
        return v >> FX_SHIFT;
    /* floor(v / 65536) for negative v, using only non-negative shifts */
    return -(int64_t)(((uint64_t)(-v) + 0xFFFFu) >> FX_SHIFT);
}

static fx_t saturate(int64_t v)
{
    if (v > INT32_MAX) return INT32_MAX;
    if (v < INT32_MIN) return INT32_MIN;
    return (fx_t)v;
}

fx_t fx_mul(fx_t a, fx_t b)
{
    return saturate(fx_floor_shr16((int64_t)a * (int64_t)b));
}

fx_t fx_div(fx_t a, fx_t b)
{
    /* Truncating division (C99 semantics are fully defined for int64_t). */
    return saturate(((int64_t)a * FX_ONE) / (int64_t)b);
}

uint64_t fx_isqrt64(uint64_t v)
{
    uint64_t res = 0;
    uint64_t bit = (uint64_t)1 << 62;

    while (bit > v)
        bit >>= 2;
    while (bit != 0) {
        if (v >= res + bit) {
            v -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

fx_t fx_sqrt(fx_t a)
{
    if (a <= 0)
        return 0;
    /* sqrt(a / 2^16) * 2^16 = sqrt(a * 2^16) */
    return (fx_t)fx_isqrt64((uint64_t)a << FX_SHIFT);
}

fx_t fx_clamp(fx_t v, fx_t lo, fx_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

fx_t fx_abs(fx_t v)
{
    return v < 0 ? (v == INT32_MIN ? INT32_MAX : -v) : v;
}

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * This file does the actual arithmetic for Q16.16 numbers (see fixed.h).
 *
 *  - fx_floor_shr16(): divides by 65536 and rounds DOWN. For negative
 *    numbers it flips the sign, divides, and flips back, because C does
 *    not promise how ">>" behaves on negative numbers.
 *  - saturate(): if a result is too big (or too small) to fit in 32 bits,
 *    it sticks to the largest (or smallest) value instead of wrapping
 *    around to garbage.
 *  - fx_mul(): multiplying two Q16.16 numbers gives a result scaled by
 *    65536 twice, so we multiply in 64 bits and then divide by 65536 once.
 *    Example: 1.5 * 1.5 -> 98304 * 98304 = 9663676416 -> /65536 = 147456
 *    = 2.25.
 *  - fx_div(): the opposite. Multiply the top by 65536 first, then divide.
 *  - fx_isqrt64(): finds the integer square root one bit at a time, from
 *    the highest bit down (the "digit-by-digit" method). No floating point
 *    is involved, so it is exact and the same on every machine.
 *  - fx_sqrt(): square root of a Q16.16 number. sqrt(a/65536) * 65536 is
 *    the same as sqrt(a * 65536), so we shift left by 16 and call isqrt.
 *    Negative input returns 0, because Snell's law treats that case as
 *    total internal reflection anyway.
 *  - fx_clamp() / fx_abs(): small helpers for limits and absolute values.
 * ===================================================================== */
