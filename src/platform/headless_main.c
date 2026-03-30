#include <stdio.h>
#include <stdlib.h>
#include "ax_math.h"
#include "ax_core.h"

// A simple testing macro to keep the output clean
#define AX_TEST(name, condition) \
    do { \
        if (condition) { \
            printf("[PASS] %s\n", name); \
        } else { \
            printf("[FAIL] %s\n", name); \
            tests_failed++; \
        } \
    } while(0)

int main(void) {
    int tests_failed = 0;
    
    printf("========================================\n");
    printf(" Axiom Engine: Headless Math Test Bench \n");
    printf("========================================\n\n");

    // -------------------------------------------------------------------------
    // 1. Test Conversions
    // -------------------------------------------------------------------------
    ax_fixed_t one = AX_INT_TO_FIXED(1);
    ax_fixed_t neg_one = AX_INT_TO_FIXED(-1);
    ax_fixed_t half = AX_FLOAT_TO_FIXED(0.5f);

    AX_TEST("Conversion: 1 == 65536 (16.16 shift)", one == 65536);
    AX_TEST("Conversion: -1 == -65536", neg_one == -65536);
    AX_TEST("Conversion: 0.5f == 32768", half == 32768);

    // -------------------------------------------------------------------------
    // 2. Test Basic Arithmetic
    // -------------------------------------------------------------------------
    ax_fixed_t two = AX_INT_TO_FIXED(2);
    ax_fixed_t three = AX_INT_TO_FIXED(3);
    
    AX_TEST("Math Add: 1 + 2 = 3", ax_math_add(one, two) == three);
    AX_TEST("Math Sub: 3 - 2 = 1", ax_math_sub(three, two) == one);

    // -------------------------------------------------------------------------
    // 3. Test Multiplication (The critical hardware check)
    // -------------------------------------------------------------------------
    // 0.5 * 0.5 = 0.25 (which is 16384 in fixed point)
    ax_fixed_t quarter = AX_FLOAT_TO_FIXED(0.25f);
    AX_TEST("Math Mul: 0.5 * 0.5 = 0.25", ax_math_mul(half, half) == quarter);

    // 2 * 0.5 = 1
    AX_TEST("Math Mul: 2.0 * 0.5 = 1.0", ax_math_mul(two, half) == one);

    // Test negative multiplication: -1 * 2 = -2
    ax_fixed_t neg_two = AX_INT_TO_FIXED(-2);
    AX_TEST("Math Mul: -1.0 * 2.0 = -2.0", ax_math_mul(neg_one, two) == neg_two);

    // -------------------------------------------------------------------------
    // 4. Test Division
    // -------------------------------------------------------------------------
    // 1 / 2 = 0.5
    AX_TEST("Math Div: 1.0 / 2.0 = 0.5", ax_math_div(one, two) == half);

    // -2 / 0.5 = -4
    ax_fixed_t neg_four = AX_INT_TO_FIXED(-4);
    AX_TEST("Math Div: -2.0 / 0.5 = -4.0", ax_math_div(neg_two, half) == neg_four);

    // -------------------------------------------------------------------------
    // 5. Test Vector Alignment (Anonymous Union Checks)
    // -------------------------------------------------------------------------
    ax_vec2_t v1 = { .x = one, .y = two };
    ax_vec2_t v2 = { .x = two, .y = three };
    ax_vec2_t v_result = ax_vec2_add(v1, v2);

    AX_TEST("Vector Add: X matches", v_result.x == three);
    AX_TEST("Vector Add: Y matches", v_result.y == AX_INT_TO_FIXED(5));
    
    // Test the raw array accessor
    AX_TEST("Vector Array Access: raw[0] == x", v_result.raw[0] == three);
    AX_TEST("Vector Array Access: raw[1] == y", v_result.raw[1] == AX_INT_TO_FIXED(5));

	// -------------------------------------------------------------------------
    // 6. Test Memory Arena Allocation & Alignment
    // -------------------------------------------------------------------------
    uint8_t backing_ram[1024]; // Simulate 1KB of system RAM
    ax_arena_t arena;
	
	// Test: Catch invalid initialization
    ax_result_t res_bad_init = ax_arena_init(&arena, NULL, 1024);
    AX_TEST("Arena Error: Catch NULL backing buffer during Init", res_bad_init == AX_ERR_INVALID_INPUT);

    // Test: Valid initialization
    ax_result_t res_good_init = ax_arena_init(&arena, backing_ram, sizeof(backing_ram));
    AX_TEST("Arena Init: AX_OK returned", res_good_init == AX_OK);
    AX_TEST("Arena Init: Offset is 0", arena.offset == 0);
    AX_TEST("Arena Init: Capacity is 1024", arena.capacity == 1024);

    // Push a single byte (alignment 1)
    uint8_t* byte_ptr = NULL;
    ax_result_t res_byte = ax_push_struct(&arena, uint8_t, &byte_ptr);
    AX_TEST("Arena Push: 1 Byte success flag", res_byte == AX_OK);
    AX_TEST("Arena Push: 1 Byte pointer valid", byte_ptr != NULL);
    if (byte_ptr) *byte_ptr = 0xFF; // Safe write

    // Push a vec3 (size 12, alignment 4). 
    ax_vec3_t* vec_ptr = NULL;
    ax_result_t res_vec = ax_push_struct(&arena, ax_vec3_t, &vec_ptr);
    AX_TEST("Arena Push: vec3 success flag", res_vec == AX_OK);
    
    // Check if the pointer actually landed on a 4-byte boundary
    AX_TEST("Arena Alignment: vec3 is 4-byte aligned", ((uintptr_t)vec_ptr % _Alignof(ax_vec3_t)) == 0);

    // Force an Out of Memory error
    uint8_t* massive_ptr = NULL;
    ax_result_t res_oom = ax_push_array(&arena, uint8_t, 2048, &massive_ptr);
    AX_TEST("Arena Error: Catch Out of Memory (OOM)", res_oom == AX_ERR_OUT_OF_MEMORY);
    AX_TEST("Arena Error: Pointer remains NULL on OOM", massive_ptr == NULL);

    // Clear the arena
    ax_arena_clear(&arena);
    AX_TEST("Arena Clear: Offset reset to 0", arena.offset == 0);
	
    printf("\n========================================\n");
    if (tests_failed == 0) {
        printf(" SUCCESS: All math checks passed!\n");
        return EXIT_SUCCESS;
    } else {
        printf(" FAILURE: %d checks failed. Do not proceed.\n", tests_failed);
        return EXIT_FAILURE;
    }
}