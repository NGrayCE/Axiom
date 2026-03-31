#include "ax_platform.h"
#include "ax_ui.h"
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
    *out_render_queue = NULL; 

    if (!g_engine.is_initialized) return AX_ERR_INVALID_INPUT;

    // 1. Zero-Cost Garbage Collection
    ax_arena_clear(&g_engine.frame_arena);

    // 2. Update Engine Time & Physics
    g_engine.current_time_us = g_engine.api.get_ticks_us();
    ax_result_t update_res = ax_engine_update(input_queue);
    if (update_res != AX_OK) return update_res;

    // 3. Create the Render Queue
    ax_render_queue_t* render_queue = NULL;
    ax_result_t q_res = ax_graphics_queue_create(&g_engine.frame_arena, 1024, &render_queue);
    if (q_res != AX_OK) return q_res;

    // Clear the background to black to catch any gaps
    ax_graphics_push_clear(render_queue, AX_COLOR_BLACK);

    // -------------------------------------------------------------------------
    // 4. Build and Solve the UI Layout
    // -------------------------------------------------------------------------
    ax_ui_context_t ui = {0};
    
    // We hardcode the screen size to 800x600 to match our SDL window for now
    ax_ui_begin_frame(&ui, &g_engine.frame_arena, AX_INT_TO_FIXED(800), AX_INT_TO_FIXED(600));
    
    // The Root splits the screen Left-to-Right
    ui.root->layout_dir = AX_UI_DIR_ROW; 

    // --- Node A: The Sidebar ---
    ax_ui_node_t* sidebar = NULL;
    ax_ui_push_node(&ui, ui.root, &sidebar);
    sidebar->width[0] = AX_SIZE_PX(200);   // Exactly 200px wide
    sidebar->width[1] = AX_SIZE_PCT(1.0f); // 100% of the screen height
    sidebar->layout_dir = AX_UI_DIR_COLUMN; // Children stack top-to-bottom
    sidebar->bg_color = AX_COLOR_MAKE(40, 45, 55, 255); // Slate Gray

    // --- Node B: Main Content Area ---
    ax_ui_node_t* main_area = NULL;
    ax_ui_push_node(&ui, ui.root, &main_area);
    main_area->width[0] = AX_SIZE_FLEX(1); // Take ALL remaining width (800 - 200 = 600px)
    main_area->width[1] = AX_SIZE_PCT(1.0f); // 100% of the screen height
    main_area->bg_color = AX_COLOR_MAKE(25, 25, 30, 255); // Darker Gray

    // --- Sidebar Children (Buttons) ---
    ax_ui_node_t* btn1 = NULL;
    ax_ui_push_node(&ui, sidebar, &btn1);
    btn1->width[0] = AX_SIZE_PCT(1.0f); // Fill the 200px sidebar
    btn1->width[1] = AX_SIZE_PX(60);    // 60px tall
    btn1->bg_color = AX_COLOR_MAKE(70, 130, 180, 255); // Steel Blue

    ax_ui_node_t* spacer = NULL;
    ax_ui_push_node(&ui, sidebar, &spacer);
    spacer->width[0] = AX_SIZE_PCT(1.0f);
    spacer->width[1] = AX_SIZE_PX(10);  // 10px invisible gap
    spacer->bg_color = 0; // Transparent

    ax_ui_node_t* btn2 = NULL;
    ax_ui_push_node(&ui, sidebar, &btn2);
    btn2->width[0] = AX_SIZE_PCT(1.0f);
    btn2->width[1] = AX_SIZE_PX(60);
    btn2->bg_color = AX_COLOR_MAKE(205, 92, 92, 255); // Indian Red

    // 5. Run the Math Solver (O(N) Complexity)
    ax_ui_solve_layout(&ui);

    // 6. Translate the computed UI coordinates into Render Commands
    ax_ui_draw(&ui, render_queue);

    // -------------------------------------------------------------------------
    // 7. Draw the Physics Object (Floating on top of the UI)
    // -------------------------------------------------------------------------
    ax_vec2_t obj_size = { .x = AX_INT_TO_FIXED(15), .y = AX_INT_TO_FIXED(15) };
    ax_graphics_push_rect(render_queue, g_engine.state->position, obj_size, AX_COLOR_WHITE);

    *out_render_queue = render_queue;
    return AX_OK;
}
