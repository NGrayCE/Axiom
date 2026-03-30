#include "ax_platform.h"
#include <stddef.h>

// -----------------------------------------------------------------------------
// 1. Internal Engine State (The Singleton Context)
// -----------------------------------------------------------------------------
// This struct is strictly private to this .c file. The host OS cannot see 
// or touch these variables, enforcing our strict decoupling boundary.

typedef struct {
    ax_system_api_t api;       // The function pointers provided by the OS
    ax_arena_t main_arena;     // Permanent memory (Assets, UI Layouts)
    ax_arena_t frame_arena;    // Temporary scratchpad (Wiped every frame)
    bool is_initialized;
    uint64_t current_time_us;
} ax_engine_context_t;

// The single, static instance of our engine state.
static ax_engine_context_t g_engine = {0};

// -----------------------------------------------------------------------------
// 2. Engine Boot Sequence
// -----------------------------------------------------------------------------
ax_result_t ax_engine_boot(const ax_system_api_t* api, 
                           void* main_memory, size_t main_size,
                           void* frame_memory, size_t frame_size) {
    
    // 1. Validate the OS actually gave us what we need
    if (api == NULL || api->get_ticks_us == NULL || api->log_message == NULL) {
        return AX_ERR_INVALID_INPUT;
    }
    if (main_memory == NULL || frame_memory == NULL) {
        return AX_ERR_INVALID_INPUT;
    }

    // 2. Prevent double-booting
    if (g_engine.is_initialized) {
        return AX_OK; 
    }

    // 3. Store the OS API
    g_engine.api = *api;

    // 4. Initialize our Memory Arenas
    ax_result_t res_main = ax_arena_init(&g_engine.main_arena, main_memory, main_size);
    if (res_main != AX_OK) return res_main;

    ax_result_t res_frame = ax_arena_init(&g_engine.frame_arena, frame_memory, frame_size);
    if (res_frame != AX_OK) return res_frame;

    g_engine.is_initialized = true;
    g_engine.current_time_us = g_engine.api.get_ticks_us();

    // 5. Signal success back to the OS via its own logging system
    g_engine.api.log_message("[AXIOM] Engine Boot Sequence Complete.");
    g_engine.api.log_message("[AXIOM] Memory Arenas Initialized and Aligned.");

    return AX_OK;
}

// -----------------------------------------------------------------------------
// 3. The Deterministic Tick
// -----------------------------------------------------------------------------
ax_result_t ax_engine_tick(const ax_input_queue_t* input_queue) {
    if (!g_engine.is_initialized) {
        return AX_ERR_INVALID_INPUT;
    }

    // 1. Update Engine Time
    // We poll the OS time purely for analytics or rendering interpolation.
    // Core game physics will strictly ignore this and use a fixed 16.66ms delta.
    g_engine.current_time_us = g_engine.api.get_ticks_us();

    // 2. Process the Input Queue chronologically
    if (input_queue != NULL && input_queue->count > 0) {
        // Enforce the buffer limit to prevent malicious or malformed OS data
        uint32_t event_count = input_queue->count;
        if (event_count > AX_MAX_INPUT_EVENTS_PER_FRAME) {
            event_count = AX_MAX_INPUT_EVENTS_PER_FRAME;
        }

        for (uint32_t i = 0; i < event_count; i++) {
            const ax_input_event_t* event = &input_queue->events[i];
            
            // Note: In Phase 4, we will pass these events to the UI Solver.
            // For now, if we get a touch down, we log it.
            if (event->type == AX_INPUT_TOUCH_DOWN) {
                g_engine.api.log_message("[AXIOM] Input: Touch Down registered.");
            }
			else if (event->type == AX_INPUT_TOUCH_UP) {
                g_engine.api.log_message("[AXIOM] Input: Touch Up registered.");
            }
        }
    }

    // 3. Update Game State (Physics, Layout) goes here...
    // ...

    // 4. Render Submission goes here...
    // ...

    // 5. The "Zero-Cost Garbage Collection"
    // At the very end of the frame, after rendering is submitted, we instantly 
    // wipe the frame scratchpad. Any memory allocated during step 2 or 3 is now gone.
    ax_arena_clear(&g_engine.frame_arena);

    return AX_OK;
}