#ifndef REFCRYPT_FIXED_H
#define REFCRYPT_FIXED_H

#include <stdint.h>

/* Q16.16 signed fixed-point: value = raw / 65536. */
typedef int32_t fx_t;

#define FX_SHIFT 16
#define FX_ONE   ((fx_t)1 << FX_SHIFT)

/* Compile-time conversion from a real literal (rounds to nearest). */
#define FX_CONST(r) ((fx_t)((r) * 65536.0 + ((r) >= 0 ? 0.5 : -0.5)))

fx_t     fx_mul(fx_t a, fx_t b);
fx_t     fx_div(fx_t a, fx_t b);      /* b must be non-zero */
fx_t     fx_sqrt(fx_t a);             /* a < 0 returns 0 */
uint64_t fx_isqrt64(uint64_t v);      /* floor(sqrt(v)) */
fx_t     fx_clamp(fx_t v, fx_t lo, fx_t hi);
fx_t     fx_abs(fx_t v);

/* Floor division of a signed 64-bit value by 2^16, independent of how the
 * compiler implements right shift of negative numbers. */
int64_t  fx_floor_shr16(int64_t v);

#endif

/* =====================================================================
 * EXPLANATION (in simple words)
 * ---------------------------------------------------------------------
 * Computers handle decimals (float/double) slightly differently on
 * different CPUs and compilers. A cipher must give EXACTLY the same bits
 * everywhere, so we store decimal numbers as plain integers instead.
 *
 * "Q16.16" means: take the real number, multiply it by 65536, and keep
 * the whole-number result. So 1.0 is stored as 65536, 1.5 as 98304 and
 * 2.417 as 158401. The top 16 bits are the integer part and the bottom
 * 16 bits are the fraction.
 *
 *  - fx_t      : the type for one of these numbers (a 32-bit integer).
 *  - FX_ONE    : the value 1.0 (= 65536).
 *  - FX_CONST  : converts a decimal you type in the code, e.g.
 *                FX_CONST(1.5), into its Q16.16 integer.
 *  - fx_mul / fx_div / fx_sqrt : multiply, divide and square-root these
 *                numbers using only integer math.
 *  - fx_isqrt64: integer square root of a big 64-bit number.
 *  - fx_clamp  : keeps a value between a low and a high limit.
 *  - fx_abs    : absolute value (removes the minus sign).
 *  - fx_floor_shr16: divides by 65536 and always rounds DOWN, even for
 *                negative numbers, so every compiler gives the same answer.
 * ===================================================================== */
