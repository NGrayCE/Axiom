#include "ax_platform.h"
#include "ax_ui.h"
#include <stddef.h>

// -----------------------------------------------------------------------------
// 1. Internal Engine State (The Singleton Context)
// -----------------------------------------------------------------------------
#include <string.h>

#define AX_MAX_ASSETS 32
typedef struct {
    char name[32];
    ax_texture_t texture;
} ax_asset_entry_t;

typedef struct {
    ax_system_api_t api;
    ax_arena_t main_arena;
    ax_arena_t frame_arena;
    bool is_initialized;
    uint64_t current_time_us;
    ax_game_state_t* state; 
	//asset registry
    ax_asset_entry_t assets[AX_MAX_ASSETS];
    uint32_t asset_count;
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
	// 1. Process Inputs for the Player (entities[0])
    ax_vec2_t player_intent = {0, 0};
    
    for (uint32_t i = 0; i < input_queue->count; i++) {
        if (input_queue->events[i].type == AX_INPUT_PLAYER_MOVE) {
            player_intent = input_queue->events[i].position;
        }
    }

    // 2. Apply Intent to Player Velocity (Speed = 5 pixels per frame)
    if (state->entities[0].is_active) {
        // In fixed-point math, multiplying a fixed 1.0 by an int 5 yields a fixed 5.0
        state->entities[0].velocity.x = player_intent.x * 2; 
        state->entities[0].velocity.y = player_intent.y * 2; 
    }
    for (int i = 0; i < AX_MAX_ENTITIES; i++) {
        ax_entity_t* e = &state->entities[i];
        if (!e->is_active) continue;

        // Apply velocity
        e->position.x += e->velocity.x;
        e->position.y += e->velocity.y;

        // Bounds collision
        if (e->position.x < 0 || e->position.x + e->size.x > state->bounds.x) {
            e->velocity.x = -e->velocity.x;
            e->position.x += e->velocity.x; 
        }
        if (e->position.y < 0 || e->position.y + e->size.y > state->bounds.y) {
            e->velocity.y = -e->velocity.y;
            e->position.y += e->velocity.y;
        }
    }

    return AX_OK;
}

// -----------------------------------------------------------------------------
// 3. Engine Boot Sequence
// -----------------------------------------------------------------------------

static const ax_texture_t* ax_engine_get_texture(const char* name) {
    for (uint32_t i = 0; i < g_engine.asset_count; i++) {
        if (strcmp(g_engine.assets[i].name, name) == 0) return &g_engine.assets[i].texture;
    }
    return NULL;
}

static ax_result_t ax_engine_load_texture(const char* name, const char* filepath) {
    if (g_engine.asset_count >= AX_MAX_ASSETS) return AX_ERR_OUT_OF_MEMORY;

    void* raw_file_data = NULL;
    size_t file_size = 0;

    // 1. Ask OS to read file (Unzips on Android, standard read on PC)
    if (g_engine.api.read_asset(filepath, &g_engine.frame_arena, &raw_file_data, &file_size) != AX_OK) {
        return AX_ERR_ASSET_LOAD_FAILED;
    }

    ax_image_t raw_image = {0};
    ax_texture_t gpu_texture = {0};

    // 2. Decode the bytes into pixels
    ax_result_t res = ax_asset_load_image(&g_engine.frame_arena, (const uint8_t*)raw_file_data, file_size, &raw_image);
    
    // 3. send to VRAM
    if (res == AX_OK) {
        res = ax_platform_upload_texture(&raw_image, &gpu_texture);
    }

    // 4. Register it
    if (res == AX_OK) {
        ax_asset_entry_t* entry = &g_engine.assets[g_engine.asset_count++];
        strncpy(entry->name, name, sizeof(entry->name) - 1);
        entry->name[sizeof(entry->name) - 1] = '\0';
        entry->texture = gpu_texture;
    }

    // 5. Wipe the frame arena clean.
    ax_arena_clear(&g_engine.frame_arena);

    return res;
}

// Internal helper to spawn an entity into the pre-allocated main arena array
static void ax_engine_spawn_entity(const char* tex_name, ax_fixed_t x, ax_fixed_t y, ax_fixed_t vx, ax_fixed_t vy, ax_fixed_t w, ax_fixed_t h) {
    if (!g_engine.state) return;
    
    for (int i = 0; i < AX_MAX_ENTITIES; i++) {
        if (!g_engine.state->entities[i].is_active) {
            ax_entity_t* e = &g_engine.state->entities[i];
            e->is_active = true;
            e->position.x = x;
            e->position.y = y;
            e->velocity.x = vx;
            e->velocity.y = vy;
            e->size.x = w;
            e->size.y = h;
            strncpy(e->texture_name, tex_name, sizeof(e->texture_name) - 1);
            e->texture_name[sizeof(e->texture_name) - 1] = '\0';
            return; // Successfully spawned
        }
    }
}

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

    // Allocate and initialize the Game State
    ax_result_t res_state = ax_push_struct(&g_engine.main_arena, ax_game_state_t, &g_engine.state);
    if (res_state != AX_OK) return res_state;

    g_engine.is_initialized = true;
    g_engine.current_time_us = g_engine.api.get_ticks_us();

	// Load the texture into the registry
    ax_result_t load_res = ax_engine_load_texture("player", "test.png");
    ax_result_t font_res = ax_engine_load_texture("font", "font.png");
	
	// Set the physical boundaries of the world
    g_engine.state->bounds.x = AX_INT_TO_FIXED(800);
    g_engine.state->bounds.y = AX_INT_TO_FIXED(600);
	// Change starting velocity to 0, 0!
	ax_engine_spawn_entity("player", AX_INT_TO_FIXED(100), AX_INT_TO_FIXED(100), 0, 0, AX_INT_TO_FIXED(50), AX_INT_TO_FIXED(50));
	ax_engine_spawn_entity("player", AX_INT_TO_FIXED(100), AX_INT_TO_FIXED(100), AX_FLOAT_TO_FIXED(2.5), AX_FLOAT_TO_FIXED(1.25), AX_INT_TO_FIXED(24), AX_INT_TO_FIXED(24));
	ax_engine_spawn_entity("player", AX_INT_TO_FIXED(100), AX_INT_TO_FIXED(100), AX_FLOAT_TO_FIXED(2.5), AX_FLOAT_TO_FIXED(1.25), AX_INT_TO_FIXED(24), AX_INT_TO_FIXED(24));
    if (load_res != AX_OK) {
        g_engine.api.log_message("[ERROR] Engine failed to load 'player' texture into registry!");
    } else {
        g_engine.api.log_message("[SUCCESS] 'player' texture loaded into registry!");
    }
	

	if (font_res != AX_OK) {
        g_engine.api.log_message("[ERROR] Engine failed to load 'font.png'!");
    } else {
        g_engine.api.log_message("[SUCCESS] Font loaded perfectly!");
    }
	
	g_engine.api.log_message("[AXIOM] Engine Boot Sequence Complete.");
    return AX_OK;
}


ax_result_t ax_engine_teardown(void) {
    // 1. Safely destroy only the textures we actually loaded
    for(uint32_t i = 0; i < g_engine.asset_count; i++) {
        ax_platform_destroy_texture(&g_engine.assets[i].texture);
    }
    
    // 2. Reset the registry count
    g_engine.asset_count = 0;

    // 3. Clear core state
    g_engine.is_initialized = false;
    g_engine.state = NULL;
    
    return AX_OK;
}

// -----------------------------------------------------------------------------
// 4. The Deterministic Tick
// -----------------------------------------------------------------------------
ax_result_t ax_engine_tick(const ax_input_queue_t* input_queue, 
                           ax_fixed_t screen_width, 
                           ax_fixed_t screen_height, 
                           ax_render_queue_t** out_render_queue) {

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

    // 4. Build and Solve the UI Layout
    ax_ui_context_t ui = {0};
    
    // Feed the dynamic size into the UI solver
    ax_ui_begin_frame(&ui, &g_engine.frame_arena, screen_width, screen_height);

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

 // 7. Draw the Entities
    for (int i = 0; i < AX_MAX_ENTITIES; i++) {
        ax_entity_t* e = &g_engine.state->entities[i];
        if (!e->is_active) continue;

        const ax_texture_t* tex = ax_engine_get_texture(e->texture_name);
        if (tex != NULL) {
            ax_vec2_t full_src_size = { AX_INT_TO_FIXED(tex->width), AX_INT_TO_FIXED(tex->height) };
            ax_graphics_push_texture(render_queue, tex, e->position, e->size, (ax_vec2_t){0,0}, full_src_size);
        } else {
            ax_graphics_push_rect(render_queue, e->position, e->size, AX_COLOR_MAKE(255, 0, 255, 255));
        }
    }
	const ax_texture_t* font = ax_engine_get_texture("font");
	ax_vec2_t text_pos = { AX_INT_TO_FIXED(50), AX_INT_TO_FIXED(50) };
	ax_graphics_push_text(render_queue, font, "HELLO AXIOM ENGINE!", text_pos, 7, 7); // Change 8, 8 to match your downloaded font's glyph size

    *out_render_queue = render_queue;
    return AX_OK;
}
