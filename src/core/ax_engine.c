#include "ax_platform.h"
#include <stddef.h>

// -----------------------------------------------------------------------------
// 1. Internal Engine State (The Singleton Context)
// -----------------------------------------------------------------------------

typedef struct {
    ax_system_api_t api;
    ax_arena_t main_arena;
    ax_arena_t frame_arena;
    bool is_initialized;
    uint64_t current_time_us;
    
    // Pointer to our live game state
    ax_game_state_t* state; 
} ax_engine_context_t;

static ax_engine_context_t g_engine = {0};

ax_result_t ax_engine_get_state(const ax_game_state_t** out_state) {
    if (out_state == NULL) {
        return AX_ERR_INVALID_INPUT;
    }
    
    // Default to NULL to prevent the caller from reading garbage data on failure
    *out_state = NULL;

    if (!g_engine.is_initialized) {
        return AX_ERR_INVALID_INPUT; 
    }

    *out_state = g_engine.state;
    return AX_OK;
}

// -----------------------------------------------------------------------------
// 2. The Deterministic Physics & Logic Update
// -----------------------------------------------------------------------------
// This function runs exactly once per tick, using pure fixed-point math.

static ax_result_t ax_engine_update(const ax_input_queue_t* input_queue) {
    ax_game_state_t* state = g_engine.state;
    state->frame_count++;

    // 1. Process Inputs (Interact with the simulation)
    if (input_queue != NULL) {
        uint32_t count = input_queue->count > AX_MAX_INPUT_EVENTS_PER_FRAME ? 
                         AX_MAX_INPUT_EVENTS_PER_FRAME : input_queue->count;
                         
        for (uint32_t i = 0; i < count; i++) {
            const ax_input_event_t* event = &input_queue->events[i];
            
            if (event->type == AX_INPUT_TOUCH_DOWN) {
                // Teleport the bouncing object directly to the touch position
                state->position = event->position;
                g_engine.api.log_message("[AXIOM] Physics: Object teleported to touch event.");
            }
        }
    }

    // 2. Apply Velocity to Position (pos = pos + vel)
    state->position = ax_vec2_add(state->position, state->velocity);

    // 3. Resolve Collisions (Bounce off the walls)
    // We use integer constants (-1) converted to fixed-point to invert velocity.
    ax_fixed_t neg_one = AX_INT_TO_FIXED(-1);
    ax_fixed_t zero = 0;

    // Check X bounds
    if (state->position.x <= zero) {
        state->position.x = zero;
        state->velocity.x = ax_math_mul(state->velocity.x, neg_one);
    } else if (state->position.x >= state->bounds.x) {
        state->position.x = state->bounds.x;
        state->velocity.x = ax_math_mul(state->velocity.x, neg_one);
    }

    // Check Y bounds
    if (state->position.y <= zero) {
        state->position.y = zero;
        state->velocity.y = ax_math_mul(state->velocity.y, neg_one);
    } else if (state->position.y >= state->bounds.y) {
        state->position.y = state->bounds.y;
        state->velocity.y = ax_math_mul(state->velocity.y, neg_one);
    }

    return AX_OK;
}

// -----------------------------------------------------------------------------
// 3. Engine Boot Sequence
// -----------------------------------------------------------------------------
ax_result_t ax_engine_boot(const ax_system_api_t* api, 
                           void* main_memory, size_t main_size,
                           void* frame_memory, size_t frame_size) {
    
    if (api == NULL || api->get_ticks_us == NULL || api->log_message == NULL) return AX_ERR_INVALID_INPUT;
    if (main_memory == NULL || frame_memory == NULL) return AX_ERR_INVALID_INPUT;
    if (g_engine.is_initialized) return AX_OK; 

    g_engine.api = *api;

    ax_result_t res_main = ax_arena_init(&g_engine.main_arena, main_memory, main_size);
    if (res_main != AX_OK) return res_main;

    ax_result_t res_frame = ax_arena_init(&g_engine.frame_arena, frame_memory, frame_size);
    if (res_frame != AX_OK) return res_frame;

    // --- NEW: Allocate and initialize the Game State ---
    ax_result_t res_state = ax_push_struct(&g_engine.main_arena, ax_game_state_t, &g_engine.state);
    if (res_state != AX_OK) return res_state;

    // Set initial physics parameters (e.g., a 100x100 arena, moving at 2.5 units per frame)
    g_engine.state->frame_count = 0;
    g_engine.state->bounds.x = AX_INT_TO_FIXED(1000);
    g_engine.state->bounds.y = AX_INT_TO_FIXED(1000);
    g_engine.state->position.x = AX_INT_TO_FIXED(50);
    g_engine.state->position.y = AX_INT_TO_FIXED(50);
    g_engine.state->velocity.x = AX_FLOAT_TO_FIXED(2.5f); 
    g_engine.state->velocity.y = AX_FLOAT_TO_FIXED(1.25f);

    g_engine.is_initialized = true;
    g_engine.current_time_us = g_engine.api.get_ticks_us();

    g_engine.api.log_message("[AXIOM] Engine Boot Sequence Complete.");
    return AX_OK;
}


ax_result_t ax_engine_teardown(void) {
    g_engine.is_initialized = false;
    g_engine.state = NULL;
    return AX_OK;
}

// -----------------------------------------------------------------------------
// 4. The Deterministic Tick
// -----------------------------------------------------------------------------
ax_result_t ax_engine_tick(const ax_input_queue_t* input_queue, ax_render_queue_t** out_render_queue) {
    if (out_render_queue == NULL) return AX_ERR_INVALID_INPUT;
    *out_render_queue = NULL; // Default to NULL for safety

    if (!g_engine.is_initialized) return AX_ERR_INVALID_INPUT;

    // 1. Zero-Cost Garbage Collection
    // Wipe last frame's scratchpad so we have fresh memory for this frame.
    ax_arena_clear(&g_engine.frame_arena);

    // 2. Update Engine Time
    g_engine.current_time_us = g_engine.api.get_ticks_us();

    // 3. Run the deterministic physics simulation
    ax_result_t update_res = ax_engine_update(input_queue);
    if (update_res != AX_OK) return update_res;

    // -------------------------------------------------------------------------
    // 4. The Render Submission Phase
    // -------------------------------------------------------------------------
    
    // Allocate a queue from the fresh frame_arena (Max 256 commands for now)
    ax_render_queue_t* render_queue = NULL;
    ax_result_t q_res = ax_graphics_queue_create(&g_engine.frame_arena, 256, &render_queue);
    if (q_res != AX_OK) return q_res;

    // Command 1: Clear the background to a dark gray
    ax_graphics_push_clear(render_queue, AX_COLOR_MAKE(30, 30, 30, 255));

    // Command 2: Draw our bouncing object as a Red Square (10x10 units)
    ax_vec2_t obj_size = { .x = AX_INT_TO_FIXED(10), .y = AX_INT_TO_FIXED(10) };
    ax_graphics_push_rect(render_queue, g_engine.state->position, obj_size, AX_COLOR_RED);

    // Hand the populated queue back to the host OS
    *out_render_queue = render_queue;

    return AX_OK;
}