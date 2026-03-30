#ifndef AXIOM_MATH_H
#define AXIOM_MATH_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup Math Deterministic Math Core
 * @brief 16.16 Fixed-Point mathematics for cross-platform determinism.
 * @{
 */

/**
 * @brief 16.16 signed fixed-point integer.
 * * Range: -32,768.0 to +32,767.99998
 * Precision: ~0.0000152588
 */
typedef int32_t ax_fixed_t;

_Static_assert(sizeof(ax_fixed_t) == 4, "Axiom requires exactly 32-bit integers for math.");
_Static_assert((-1 >> 1) == -1, "Axiom requires compiler support for Arithmetic Right Shift.");

/** @brief The number of fractional bits in the fixed-point representation. */
#define AX_FIXED_SHIFT 16
/** @brief Mask to extract only the fractional portion of a fixed-point number. */
#define AX_FIXED_FRACTION_MASK 0x0000FFFF
/** @brief The fixed-point representation of 1.0. */
#define AX_FIXED_ONE  (1 << AX_FIXED_SHIFT)
/** @brief The fixed-point representation of 0.5. */
#define AX_FIXED_HALF (1 << (AX_FIXED_SHIFT - 1))

/**
 * @brief Converts a standard integer to a fixed-point number.
 * @param i The integer to convert.
 * @return The fixed-point representation.
 */
#define AX_INT_TO_FIXED(i) ((ax_fixed_t)((i) << AX_FIXED_SHIFT))

/**
 * @brief Converts a fixed-point number to a standard truncated integer.
 * @param f The fixed-point number.
 * @return The truncated integer representation.
 */
#define AX_FIXED_TO_INT(f) ((int32_t)((f) >> AX_FIXED_SHIFT))

/**
 * @brief Converts a float to a fixed-point number. 
 * @warning Do not use during the deterministic simulation loop.
 * @param f The float to convert.
 * @return The fixed-point representation.
 */
#define AX_FLOAT_TO_FIXED(f) ((ax_fixed_t)((f) * (1 << AX_FIXED_SHIFT)))

/**
 * @brief Converts a fixed-point number to a float.
 * @warning Do not use during the deterministic simulation loop.
 * @param f The fixed-point number.
 * @return The floating-point representation.
 */
#define AX_FIXED_TO_FLOAT(f) ((float)(f) / (float)(1 << AX_FIXED_SHIFT))

/**
 * @brief Adds two fixed-point numbers.
 * @param a The first operand.
 * @param b The second operand.
 * @return The fixed-point sum.
 */
static inline ax_fixed_t ax_math_add(ax_fixed_t a, ax_fixed_t b) {
    return a + b;
}

/**
 * @brief Subtracts the second fixed-point number from the first.
 * @param a The minuend.
 * @param b The subtrahend.
 * @return The fixed-point difference.
 */
static inline ax_fixed_t ax_math_sub(ax_fixed_t a, ax_fixed_t b) {
    return a - b;
}

/**
 * @brief Multiplies two fixed-point numbers.
 * @details Utilizes a 64-bit intermediate cast to prevent overflow before shifting.
 * @param a The first multiplicand.
 * @param b The second multiplicand.
 * @return The fixed-point product.
 */
static inline ax_fixed_t ax_math_mul(ax_fixed_t a, ax_fixed_t b) {
    return (ax_fixed_t)(((int64_t)a * (int64_t)b) >> AX_FIXED_SHIFT);
}

/**
 * @brief Divides the first fixed-point number by the second.
 * @warning The caller must ensure that parameter 'b' is not zero.
 * @param a The dividend.
 * @param b The divisor.
 * @return The fixed-point quotient.
 */
static inline ax_fixed_t ax_math_div(ax_fixed_t a, ax_fixed_t b) {
    return (ax_fixed_t)((((int64_t)a) << AX_FIXED_SHIFT) / b);
}

/**
 * @brief A 2D vector utilizing fixed-point coordinates.
 */
typedef struct {
    union {
        struct { ax_fixed_t x, y; };
        ax_fixed_t raw[2];
    };
} ax_vec2_t;

/**
 * @brief A 3D vector utilizing fixed-point coordinates.
 */
typedef struct {
    union {
        struct { ax_fixed_t x, y, z; };
        struct { ax_vec2_t xy; ax_fixed_t _ignored_z; }; 
        ax_fixed_t raw[3];
    };
} ax_vec3_t;

_Static_assert(sizeof(ax_vec2_t) == 8, "ax_vec2_t is improperly padded.");
_Static_assert(sizeof(ax_vec3_t) == 12, "ax_vec3_t is improperly padded.");

/**
 * @brief Adds two 2D vectors together.
 * @param a The first vector.
 * @param b The second vector.
 * @return The resulting vector sum.
 */
static inline ax_vec2_t ax_vec2_add(ax_vec2_t a, ax_vec2_t b) {
    return (ax_vec2_t){ .x = a.x + b.x, .y = a.y + b.y };
}

/** @} */

#endif // AXIOM_MATH_H