#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>

#include "ax_platform.h"
#include "ax_graphics.h"

// -----------------------------------------------------------------------------
// 1. The OS System API Implementation
// -----------------------------------------------------------------------------

static uint64_t sdl_get_ticks_us(void) {
    // SDL3 returns nanoseconds. We divide by 1000 for our microsecond engine clock.
    return SDL_GetTicksNS() / 1000;
}

static void sdl_log_message(const char* message) {
    SDL_Log("[Engine] %s", message);
}

static ax_result_t sdl_read_asset(const char* filename, ax_arena_t* arena, void** out_buffer, size_t* out_size) {
    if (!filename || !arena || !out_buffer || !out_size) return AX_ERR_INVALID_INPUT;

    void* temp_data = NULL;
    size_t file_size = 0;

    // Attempt 1: Use the absolute Base Path (Solves Windows/Mac build folder CWD issues)
    char* base_path = SDL_GetBasePath();
    if (base_path) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s%s", base_path, filename);
        temp_data = SDL_LoadFile(full_path, &file_size);
        SDL_free(base_path);
    }

    // Attempt 2: Fallback to the raw relative filename (Solves Android APK Asset loading)
    if (!temp_data) {
        temp_data = SDL_LoadFile(filename, &file_size);
    }

    // If both failed, we legitimately cannot find the file
    if (!temp_data) {
        SDL_Log("SDL Failed to read asset '%s': %s", filename, SDL_GetError());
        return AX_ERR_ASSET_LOAD_FAILED;
    }

    // Allocate permanent engine memory and copy the data over
    void* arena_data = NULL;
    ax_result_t res = ax_arena_push(arena, file_size, 1, &arena_data);
    
    if (res == AX_OK) {
        memcpy(arena_data, temp_data, file_size);
        *out_buffer = arena_data;
        *out_size = file_size;
    }

    SDL_free(temp_data); 
    return res;
}

// -----------------------------------------------------------------------------
// 2. The Asset loader
// -----------------------------------------------------------------------------
SDL_Renderer* g_renderer = NULL;

ax_result_t ax_platform_upload_texture(const ax_image_t* image, ax_texture_t* out_texture) {
    // 1. Guard against bad inputs
    if (!image || !image->pixels || !out_texture) {
        return AX_ERR_INVALID_INPUT;
    }

    // 2. Attempt VRAM Allocation
    SDL_Texture* sdl_tex = SDL_CreateTexture(
        g_renderer, 
        SDL_PIXELFORMAT_RGBA32, 
        SDL_TEXTUREACCESS_STATIC, 
        (int)image->width, 
        (int)image->height
    );

    if (!sdl_tex) {
        // The GPU refused to give us memory (e.g., texture size exceeded hardware limits)
        return AX_ERR_GPU_ALLOC_FAILED;
    }

    // 3. Attempt VRAM Upload
   int pitch = (int)(image->width * 4);
   bool upload_success = SDL_UpdateTexture(sdl_tex, NULL, image->pixels, pitch);

    if (!upload_success) {
        // The bus transfer failed. Clean up the empty texture container so we don't leak VRAM.
		SDL_Log("CRITICAL GPU ERROR: %s", SDL_GetError());
        SDL_Log("Attempted to upload %dx%d image with pitch %d", image->width, image->height, pitch);
        SDL_DestroyTexture(sdl_tex);
        return AX_ERR_GPU_UPLOAD_FAILED;
    }

    // 4. Commit handle
    out_texture->width = image->width;
    out_texture->height = image->height;
    out_texture->platform_handle = (void*)sdl_tex; 

    return AX_OK;
}

ax_result_t ax_platform_destroy_texture(ax_texture_t* texture) {
    if (!texture) {
        return AX_ERR_INVALID_INPUT;
    }

    if (texture->platform_handle) {
        SDL_DestroyTexture((SDL_Texture*)texture->platform_handle);
        texture->platform_handle = NULL;
    }

    texture->width = 0;
    texture->height = 0;

    return AX_OK;
}
// -----------------------------------------------------------------------------
// 3. The Main Application Loop
// -----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    // 1. Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return -1;
    }

    // 2. Create the Window and Renderer
    SDL_Window* window = NULL;

    
    // We want a 800x600 window. SDL3 creates a VSynced renderer by default here.
    if (!SDL_CreateWindowAndRenderer("Axiom Engine", 800, 600, 0, &window, &g_renderer)) {
        SDL_Log("Failed to create window/renderer: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

	// Create the memory arenas
    size_t main_size = 1024 * 1024 * 16;
    size_t frame_size = 1024 * 1024 * 16;
	
    //calloc here to guarantee zero-initialized state
    void* main_ram = calloc(1, main_size);     
    void* frame_ram = calloc(1, frame_size);   

    ax_system_api_t api = {
        .get_ticks_us = sdl_get_ticks_us,
        .log_message = sdl_log_message,
        .read_asset = sdl_read_asset
    };

    // Boot the Engine
    if (ax_engine_boot(&api, main_ram, main_size, frame_ram, frame_size) != AX_OK) {
        SDL_Log("Engine failed to boot!");
        return -1;
    }

    bool running = true;
    while (running) {
        SDL_Event event;
        ax_input_queue_t input_queue = {0};

        // Query the physical screen size dynamically
        int screen_w = 0, screen_h = 0;
        SDL_GetWindowSizeInPixels(window, &screen_w, &screen_h);

        // Poll OS events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            // Desktop Mouse Support
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (input_queue.count < AX_MAX_INPUT_EVENTS_PER_FRAME) {
                    ax_input_event_t* ax_event = &input_queue.events[input_queue.count++];
                    ax_event->type = AX_INPUT_TOUCH_DOWN;
                    ax_event->position.x = AX_FLOAT_TO_FIXED(event.button.x);
                    ax_event->position.y = AX_FLOAT_TO_FIXED(event.button.y);
                    ax_event->timestamp_us = (uint32_t)sdl_get_ticks_us();
                }
            }
            // Mobile Touch Support (Normalized Coordinates)
            else if (event.type == SDL_EVENT_FINGER_DOWN) {
                if (input_queue.count < AX_MAX_INPUT_EVENTS_PER_FRAME) {
                    ax_input_event_t* ax_event = &input_queue.events[input_queue.count++];
                    ax_event->type = AX_INPUT_TOUCH_DOWN;
                    
                    // Convert normalized [0..1] finger position to physical pixels
                    float pixel_x = event.tfinger.x * screen_w;
                    float pixel_y = event.tfinger.y * screen_h;
                    
                    ax_event->position.x = AX_FLOAT_TO_FIXED(pixel_x);
                    ax_event->position.y = AX_FLOAT_TO_FIXED(pixel_y);
                    ax_event->timestamp_us = (uint32_t)sdl_get_ticks_us();
                }
            }
        }
		//PLAYER MOVEMENT
		// 1. Poll the raw hardware state of the keyboard
		const uint8_t* keys = SDL_GetKeyboardState(NULL);
		int move_x = 0;
		int move_y = 0;

		// 2. Map WASD or Arrow Keys to a mathematical vector
		if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) move_x += 1;
		if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) move_x -= 1;
		if (keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]) move_y += 1;
		if (keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]) move_y -= 1;

		// 3. If the player is pushing a direction, push it to the Engine's Input Queue
		if (move_x != 0 || move_y != 0) {
			// Assuming your event struct is named ax_input_event_t
			ax_input_event_t move_event = {0}; 
			move_event.type = AX_INPUT_PLAYER_MOVE;
			
			// Convert our integers into engine-safe 16.16 fixed-point numbers
			move_event.position.x = AX_INT_TO_FIXED(move_x);
			move_event.position.y = AX_INT_TO_FIXED(move_y);
			
			// Push onto the queue (ensure you don't exceed your queue's max capacity!)
			input_queue.events[input_queue.count++] = move_event;
		}
		
        // Tick the Engine
        ax_render_queue_t* render_queue = NULL;
        if (ax_engine_tick(&input_queue, AX_INT_TO_FIXED(screen_w), AX_INT_TO_FIXED(screen_h), &render_queue) != AX_OK) {
            SDL_Log("Engine tick failed!");
            break;
        }
		
        // 5. Render the Engine's Output
        if (render_queue != NULL) {
            for (uint32_t i = 0; i < render_queue->count; i++) {
                ax_render_cmd_t* cmd = &render_queue->commands[i];
                switch (cmd->type) {
                    case AX_RENDER_CMD_CLEAR: {
                        uint8_t r = cmd->as.clear.color & 0xFF;
                        uint8_t g = (cmd->as.clear.color >> 8) & 0xFF;
                        uint8_t b = (cmd->as.clear.color >> 16) & 0xFF;
                        uint8_t a = (cmd->as.clear.color >> 24) & 0xFF;
                        SDL_SetRenderDrawColor(g_renderer, r, g, b, a);
                        SDL_RenderClear(g_renderer);
                        break;
                    }
                    case AX_RENDER_CMD_DRAW_RECT: {
                        uint8_t r = cmd->as.draw_rect.color & 0xFF;
                        uint8_t g = (cmd->as.draw_rect.color >> 8) & 0xFF;
                        uint8_t b = (cmd->as.draw_rect.color >> 16) & 0xFF;
                        uint8_t a = (cmd->as.draw_rect.color >> 24) & 0xFF;
                        
                        SDL_FRect rect;
                        rect.x = AX_FIXED_TO_FLOAT(cmd->as.draw_rect.position.x);
                        rect.y = AX_FIXED_TO_FLOAT(cmd->as.draw_rect.position.y);
                        rect.w = AX_FIXED_TO_FLOAT(cmd->as.draw_rect.size.x);
                        rect.h = AX_FIXED_TO_FLOAT(cmd->as.draw_rect.size.y);

                        SDL_SetRenderDrawColor(g_renderer, r, g, b, a);
                        SDL_RenderFillRect(g_renderer, &rect);
                        break;
                    }
                    case AX_RENDER_CMD_DRAW_TEXTURE: {
						SDL_Texture* sdl_tex = (SDL_Texture*)cmd->as.draw_texture.texture->platform_handle;
						
						// Define the slice of the image to use (convert fixed-point back to floats)
						SDL_FRect src_rect;
						src_rect.x = AX_FIXED_TO_FLOAT(cmd->as.draw_texture.src_pos.x);
						src_rect.y = AX_FIXED_TO_FLOAT(cmd->as.draw_texture.src_pos.y);
						src_rect.w = AX_FIXED_TO_FLOAT(cmd->as.draw_texture.src_size.x);
						src_rect.h = AX_FIXED_TO_FLOAT(cmd->as.draw_texture.src_size.y);

						// Define where it goes on the screen
						SDL_FRect dst_rect;
						dst_rect.x = AX_FIXED_TO_FLOAT(cmd->as.draw_texture.dst_pos.x);
						dst_rect.y = AX_FIXED_TO_FLOAT(cmd->as.draw_texture.dst_pos.y);
						dst_rect.w = AX_FIXED_TO_FLOAT(cmd->as.draw_texture.dst_size.x);
						dst_rect.h = AX_FIXED_TO_FLOAT(cmd->as.draw_texture.dst_size.y);

						// Pass BOTH rectangles to the GPU
						SDL_RenderTexture(g_renderer, sdl_tex, &src_rect, &dst_rect);
						break;
					}
                    default:
                        break;
                }
            }
        }

        // Swap the buffers to display the frame
        SDL_RenderPresent(g_renderer);
    }

    // Clean up
    ax_engine_teardown();
    free(main_ram);
    free(frame_ram);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
