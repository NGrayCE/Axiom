#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ax_math.h"
#include "ax_core.h"
#include "ax_platform.h"

// -----------------------------------------------------------------------------
// Test Macro
// -----------------------------------------------------------------------------
#define AX_TEST(name, condition) \
    do { \
        if (condition) { \
            printf("[PASS] %s\n", name); \
        } else { \
            printf("[FAIL] %s\n", name); \
            failed_tests++; \
        } \
    } while(0)

static int failed_tests = 0;

// -----------------------------------------------------------------------------
// Suite 1: Math Core
// -----------------------------------------------------------------------------
static void run_math_tests(void) {
    printf("\n--- Running Math Tests ---\n");
    ax_fixed_t one = AX_INT_TO_FIXED(1);
    ax_fixed_t half = AX_FLOAT_TO_FIXED(0.5f);
    ax_fixed_t quarter = AX_FLOAT_TO_FIXED(0.25f);
    ax_fixed_t two = AX_INT_TO_FIXED(2);

    AX_TEST("Math Mul: 0.5 * 0.5 = 0.25", ax_math_mul(half, half) == quarter);
    AX_TEST("Math Div: 1.0 / 2.0 = 0.5", ax_math_div(one, two) == half);
    
    ax_vec2_t v1 = { .x = one, .y = two };
    ax_vec2_t v2 = { .x = two, .y = one };
    ax_vec2_t v_res = ax_vec2_add(v1, v2);
    AX_TEST("Vector Add: X and Y match", v_res.x == AX_INT_TO_FIXED(3) && v_res.y == AX_INT_TO_FIXED(3));
}

// -----------------------------------------------------------------------------
// Suite 2: Memory Arena
// -----------------------------------------------------------------------------
static void run_arena_tests(void) {
    printf("\n--- Running Memory Tests ---\n");
    uint8_t backing_ram[1024];
    ax_arena_t arena;
    
    AX_TEST("Arena Init: AX_OK", ax_arena_init(&arena, backing_ram, sizeof(backing_ram)) == AX_OK);

    ax_vec3_t* vec_ptr = NULL;
    ax_result_t res = ax_push_struct(&arena, ax_vec3_t, &vec_ptr);
    AX_TEST("Arena Push: Success", res == AX_OK && vec_ptr != NULL);
    AX_TEST("Arena Alignment: 4-byte boundary", ((uintptr_t)vec_ptr % _Alignof(ax_vec3_t)) == 0);

    uint8_t* massive_ptr = NULL;
    AX_TEST("Arena Error: Catch OOM", ax_push_array(&arena, uint8_t, 2048, &massive_ptr) == AX_ERR_OUT_OF_MEMORY);
}

// -----------------------------------------------------------------------------
// Suite 3: Platform & Engine Boundary (The Fake OS)
// -----------------------------------------------------------------------------

// Fake OS Function: A dummy clock that ticks forward slightly every time it's called
static uint64_t fake_system_clock = 1000000;
static uint64_t fake_os_get_ticks_us(void) {
    fake_system_clock += 16666; // Simulate ~16.6ms passing
    return fake_system_clock;
}

// Fake OS Function: Route engine logs directly to desktop standard output
static void fake_os_log(const char* message) {
    printf("   > OS_CONSOLE: %s\n", message);
}

// Fake OS Function: Asset reading (stubbed out for now)
static ax_result_t fake_os_read_asset(const char* filename, void** out_buffer, size_t* out_size) {
    (void)filename; (void)out_buffer; (void)out_size; // Suppress unused warnings
    return AX_ERR_INVALID_INPUT; 
}

static void run_engine_boot_tests(void) {
    printf("\n--- Running Engine Boundary Tests ---\n");

    // 1. Setup the Fake OS API Contract
    ax_system_api_t fake_api = {
        .get_ticks_us = fake_os_get_ticks_us,
        .log_message = fake_os_log,
        .read_asset = fake_os_read_asset
    };

    // 2. Allocate the System RAM (Simulating what Android/iOS would do)
    // We allocate 1MB for permanent storage, and 256KB for the frame scratchpad.
    void* main_ram = malloc(1024 * 1024);     
    void* frame_ram = malloc(256 * 1024);     
    
    AX_TEST("Fake OS: RAM Allocated", main_ram != NULL && frame_ram != NULL);

    // 3. Test the Boot Sequence
    ax_result_t boot_res = ax_engine_boot(&fake_api, main_ram, 1024 * 1024, frame_ram, 256 * 1024);
    AX_TEST("Engine Boot: AX_OK", boot_res == AX_OK);

    // Clean up our Fake OS memory
    free(main_ram);
    free(frame_ram);
}

// -----------------------------------------------------------------------------
// Suite 4: Input Queue & Deterministic Tick
// -----------------------------------------------------------------------------
static void run_input_tick_tests(void) {
    printf("\n--- Running Input Tick Tests ---\n");

    // 1. Manually construct a multi-event input queue
    // We simulate a "Fast Tap": Down, then Up, within a single frame.
    ax_input_queue_t queue = {0};
    
    // Event 0: Touch Down at (10, 10)
    queue.events[0].type = AX_INPUT_TOUCH_DOWN;
    queue.events[0].position = (ax_vec2_t){ .x = AX_INT_TO_FIXED(10), .y = AX_INT_TO_FIXED(10) };
    queue.events[0].timestamp_us = 1000500;
    
    // Event 1: Touch Up at (10, 10)
    queue.events[1].type = AX_INPUT_TOUCH_UP;
    queue.events[1].position = (ax_vec2_t){ .x = AX_INT_TO_FIXED(10), .y = AX_INT_TO_FIXED(10) };
    queue.events[1].timestamp_us = 1000520;
    
    queue.count = 2;

    // 2. Trigger the tick
    // The engine should log "Touch Down registered" even though the finger is 
    // already "up" by the time the tick finishes.
    printf("   [Action] Feeding 2 events (Down/Up) into ax_engine_tick...\n");
    ax_result_t tick_res = ax_engine_tick(&queue);
    
    AX_TEST("Engine Tick: Processed multi-event queue", tick_res == AX_OK);

    // 3. Test Empty Queue (Idle Frame)
    ax_input_queue_t empty_queue = { .count = 0 };
    AX_TEST("Engine Tick: Processed empty queue (Idle)", ax_engine_tick(&empty_queue) == AX_OK);
}

// -----------------------------------------------------------------------------
// Main Execution
// -----------------------------------------------------------------------------
int main(void) {
    printf("========================================\n");
    printf(" Axiom Engine: Headless Test Bench \n");
    printf("========================================\n");

    run_math_tests();
    run_arena_tests();
    run_engine_boot_tests();
	run_input_tick_tests();
	
    printf("\n========================================\n");
    if (failed_tests == 0) {
        printf(" SUCCESS: All %d test suites passed!\n", 4); // Update count if adding suites
        return EXIT_SUCCESS;
    } else {
        printf(" FAILURE: %d individual checks failed.\n", failed_tests);
        return EXIT_FAILURE;
    }
}