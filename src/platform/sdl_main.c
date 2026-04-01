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

static ax_result_t sdl_read_asset(const char* filename, void** out_buffer, size_t* out_size) {
    (void)filename; (void)out_buffer; (void)out_size;
    return AX_ERR_INVALID_INPUT; // Stubbed for now
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

    // 3. Allocate Engine Memory
    void* main_ram = malloc(1024 * 1024);     // 1MB
    void* frame_ram = malloc(256 * 1024);     // 256KB

    ax_system_api_t api = {
        .get_ticks_us = sdl_get_ticks_us,
        .log_message = sdl_log_message,
        .read_asset = sdl_read_asset
    };

    // Boot the Engine
    if (ax_engine_boot(&api, main_ram, 1024 * 1024, frame_ram, 256 * 1024) != AX_OK) {
        SDL_Log("Engine failed to boot!");
        return -1;
    }
	// ==============================================================================
	// 1. INITIALIZATION
	// ==============================================================================

	size_t temp_ram_size = 1024 * 1024 * 4;
    uint8_t* temp_img_ram = (uint8_t*)malloc(temp_ram_size); 
    
    ax_arena_t temp_arena;
    ax_arena_init(&temp_arena, temp_img_ram, temp_ram_size);

    ax_image_t raw_image = {0};
    ax_texture_t test_texture = {0};

    // Attempt to load the image into the temporary CPU arena
    ax_result_t load_status = ax_asset_load_image(&temp_arena, "C:/dev/Axiom/add_image/build/Debug/test.png", &raw_image);
	
	if (load_status == AX_OK) {
		// If it loaded, blast the pixels to the GPU
		ax_result_t upload_status = ax_platform_upload_texture(&raw_image, &test_texture);
		
		if (upload_status != AX_OK) {
			SDL_Log("Engine Error: GPU Upload failed with code %d", upload_status);
		} else {
			SDL_Log("Engine Success: Texture loaded and sitting in VRAM!");
		}
	} else {
		SDL_Log("Engine Error: Failed to find or decode test.png. Code: %d", load_status);
	}
	
    // We can completely delete the temporary CPU memory before the game loop starts!
    free(temp_img_ram);
	
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

        // Tick the Engine
        ax_render_queue_t* render_queue = NULL;
        if (ax_engine_tick(&input_queue, AX_INT_TO_FIXED(screen_w), AX_INT_TO_FIXED(screen_h), &render_queue) != AX_OK) {
            SDL_Log("Engine tick failed!");
            break;
        }
		
		//Queue the texture
		if (render_queue != NULL && test_texture.platform_handle != NULL) {
            // Use fixed-point math, not floats!
            ax_vec2_t pos = { AX_INT_TO_FIXED(100), AX_INT_TO_FIXED(100) };
            ax_vec2_t size = { AX_INT_TO_FIXED(test_texture.width), AX_INT_TO_FIXED(test_texture.height) };
            ax_graphics_push_texture(render_queue, &test_texture, pos, size);
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
                        // Extract the raw SDL handle and draw it
                        SDL_Texture* sdl_tex = (SDL_Texture*)cmd->as.draw_texture.texture->platform_handle;
                        if (sdl_tex) {
                            SDL_FRect dest_rect = {
                                AX_FIXED_TO_FLOAT(cmd->as.draw_texture.position.x),
                                AX_FIXED_TO_FLOAT(cmd->as.draw_texture.position.y),
                                AX_FIXED_TO_FLOAT(cmd->as.draw_texture.size.x),
                                AX_FIXED_TO_FLOAT(cmd->as.draw_texture.size.y)
                            };
                            SDL_RenderTexture(g_renderer, sdl_tex, NULL, &dest_rect);
                        }
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
	// Safely destroy the GPU handle before the program closes
	ax_platform_destroy_texture(&test_texture);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
