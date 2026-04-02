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

// Fake OS Function: A dummy clock
static uint64_t fake_system_clock = 1000000;
static uint64_t fake_os_get_ticks_us(void) {
    fake_system_clock += 16666; 
    return fake_system_clock;
}

// Fake OS Function: Log routing
static void fake_os_log(const char* message) {
    printf("   > OS_CONSOLE: %s\n", message);
}

// Fake OS Function: Asset reading (Mocks a 1x1 PNG file)
static ax_result_t fake_os_read_asset(const char* filename, ax_arena_t* arena, void** out_buffer, size_t* out_size) {
    (void)filename;
    
    // A mathematically valid 1x1 pixel transparent PNG file
    static const uint8_t fake_png[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
        0x0a, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x00, 0x01, 0x00, 0x00,
        0x05, 0x00, 0x01, 0x0d, 0x0a, 0x2d, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x49,
        0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
    };

    void* mem = NULL;
    ax_result_t res = ax_arena_push(arena, sizeof(fake_png), 1, &mem);
    
    if (res == AX_OK) {
        memcpy(mem, fake_png, sizeof(fake_png));
        *out_buffer = mem;
        *out_size = sizeof(fake_png);
    }
    return res;
}

// -----------------------------------------------------------------------------
// Dummy GPU Boundaries (Prevents Linker Errors)
// -----------------------------------------------------------------------------
ax_result_t ax_platform_upload_texture(const ax_image_t* image, ax_texture_t* out_texture) {
    if (!image || !out_texture) return AX_ERR_INVALID_INPUT;
    
    // Pretend we uploaded to the GPU and give it a fake platform handle
    out_texture->width = image->width;
    out_texture->height = image->height;
    out_texture->platform_handle = (void*)(uintptr_t)0xDEADBEEF; 
    return AX_OK;
}

ax_result_t ax_platform_destroy_texture(ax_texture_t* texture) {
    if (texture) texture->platform_handle = NULL;
    return AX_OK;
}

static uint8_t fake_os_main_ram[1024 * 1024];     // 1MB
static uint8_t fake_os_frame_ram[256 * 1024];     // 256KB

static void run_engine_boot_tests(void) {
    printf("\n--- Running Engine Boundary Tests ---\n");

    ax_system_api_t fake_api = {
        .get_ticks_us = fake_os_get_ticks_us,
        .log_message = fake_os_log,
        .read_asset = fake_os_read_asset
    };

    // Boot the engine using the static RAM
    ax_result_t boot_res = ax_engine_boot(&fake_api, 
                                          fake_os_main_ram, sizeof(fake_os_main_ram), 
                                          fake_os_frame_ram, sizeof(fake_os_frame_ram));
                                          
    AX_TEST("Engine Boot: AX_OK", boot_res == AX_OK);
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
	// Update both tick calls to capture the render queue
    ax_render_queue_t* render_queue = NULL;
	ax_result_t tick_res = ax_engine_tick(&queue, AX_INT_TO_FIXED(800), AX_INT_TO_FIXED(600), &render_queue);
    
    AX_TEST("Engine Tick: Processed multi-event queue", tick_res == AX_OK);

    // Test Empty Queue
    ax_input_queue_t empty_queue = { .count = 0 };
    AX_TEST("Engine Tick: Processed empty queue", ax_engine_tick(&empty_queue, AX_INT_TO_FIXED(800), AX_INT_TO_FIXED(600), &render_queue) == AX_OK);
}

// -----------------------------------------------------------------------------
// Suite 5: The Determinism Crucible (600 Frame Simulation)
// -----------------------------------------------------------------------------
static void run_determinism_test(void) {
    printf("\n--- Running 10-Second Determinism Crucible ---\n");

    // ---Cleanly reboot the engine to clear Suite 4's inputs ---
    ax_engine_teardown();
    
    ax_system_api_t fake_api = {
        .get_ticks_us = fake_os_get_ticks_us,
        .log_message = fake_os_log,
        .read_asset = fake_os_read_asset
    };
    
    ax_engine_boot(&fake_api, 
                   fake_os_main_ram, sizeof(fake_os_main_ram), 
                   fake_os_frame_ram, sizeof(fake_os_frame_ram));
    // --------------------------------------------------------------------

    ax_input_queue_t empty_queue = { .count = 0 };
    ax_render_queue_t* render_queue = NULL;

    printf("   [Action] Simulating 600 frames (10 seconds at 60Hz)...\n");
    
    for (uint32_t i = 0; i < 600; i++) {
        ax_result_t res = ax_engine_tick(&empty_queue, AX_INT_TO_FIXED(800), AX_INT_TO_FIXED(600), &render_queue);
        if (res != AX_OK) {
            printf("[FAIL] Engine tick failed at frame %d\n", i);
            failed_tests++;
            return;
        }
    }

    // --- Graphics API Assertion ---
    AX_TEST("Graphics: Queue generated", render_queue != NULL);
    AX_TEST("Graphics: Queue contains UI and Physics", render_queue->count >= 6);
    
    // The physics object is always the LAST command pushed in our tick function
    ax_render_cmd_t* phys_cmd = &render_queue->commands[render_queue->count - 1];
    
    AX_TEST("Graphics: Last command is DRAW_TEXTURE", phys_cmd->type == AX_RENDER_CMD_DRAW_TEXTURE);
    
    const ax_game_state_t* final_state = NULL;
    ax_engine_get_state(&final_state);
    
    ax_vec2_t render_pos = phys_cmd->as.draw_texture.position;
    AX_TEST("Graphics: Render coords match physics state", 
            render_pos.x == final_state->position.x && render_pos.y == final_state->position.y);
			
    // -------------------------------------------------------------------------
    // The "Golden Master" Assertion
    // -------------------------------------------------------------------------
    float final_x = AX_FIXED_TO_FLOAT(final_state->position.x);
    float final_y = AX_FIXED_TO_FLOAT(final_state->position.y);
    
    printf("   [Result] Final Position: X = %.3f, Y = %.3f\n", final_x, final_y);

    // Update expected values to match the new bounds provided by the dynamic UI
    ax_fixed_t expected_x = AX_INT_TO_FIXED(450); 
    ax_fixed_t expected_y = AX_INT_TO_FIXED(800); 

    AX_TEST("Determinism: X coordinate matches Golden Master", final_state->position.x == expected_x);
    AX_TEST("Determinism: Y coordinate matches Golden Master", final_state->position.y == expected_y);
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
	run_determinism_test();
	
    printf("\n========================================\n");
    if (failed_tests == 0) {
        printf(" SUCCESS: All %d test suites passed!\n", 5); // Update count if adding suites
        return EXIT_SUCCESS;
    } else {
        printf(" FAILURE: %d individual checks failed.\n", failed_tests);
        return EXIT_FAILURE;
    }
}