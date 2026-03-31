#include <SDL3/SDL.h>
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
// 2. The Main Application Loop
// -----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    // 1. Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return -1;
    }

    // 2. Create the Window and Renderer
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    
    // We want a 800x600 window. SDL3 creates a VSynced renderer by default here.
    if (!SDL_CreateWindowAndRenderer("Axiom Engine", 800, 600, 0, &window, &renderer)) {
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

    // 4. The Game Loop
    bool running = true;
    while (running) {
        SDL_Event event;
        ax_input_queue_t input_queue = {0};

        // Poll OS events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            // Translate Mouse Clicks to Touch Events
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (input_queue.count < AX_MAX_INPUT_EVENTS_PER_FRAME) {
                    ax_input_event_t* ax_event = &input_queue.events[input_queue.count++];
                    ax_event->type = AX_INPUT_TOUCH_DOWN;
                    // Convert float mouse coords to fixed-point
                    ax_event->position.x = AX_FLOAT_TO_FIXED(event.button.x);
                    ax_event->position.y = AX_FLOAT_TO_FIXED(event.button.y);
                    ax_event->timestamp_us = (uint32_t)sdl_get_ticks_us();
                }
            }
        }

        // Tick the Engine
        ax_render_queue_t* render_queue = NULL;
        if (ax_engine_tick(&input_queue, &render_queue) != AX_OK) {
            SDL_Log("Engine tick failed!");
            break;
        }

        // 5. Render the Engine's Output
        if (render_queue != NULL) {
            for (uint32_t i = 0; i < render_queue->count; i++) {
                ax_render_cmd_t* cmd = &render_queue->commands[i];

                if (cmd->type == AX_RENDER_CMD_CLEAR) {
                    uint8_t r = cmd->clear.color & 0xFF;
                    uint8_t g = (cmd->clear.color >> 8) & 0xFF;
                    uint8_t b = (cmd->clear.color >> 16) & 0xFF;
                    uint8_t a = (cmd->clear.color >> 24) & 0xFF;
                    SDL_SetRenderDrawColor(renderer, r, g, b, a);
                    SDL_RenderClear(renderer);
                }
                else if (cmd->type == AX_RENDER_CMD_DRAW_RECT) {
                    uint8_t r = cmd->draw_rect.color & 0xFF;
                    uint8_t g = (cmd->draw_rect.color >> 8) & 0xFF;
                    uint8_t b = (cmd->draw_rect.color >> 16) & 0xFF;
                    uint8_t a = (cmd->draw_rect.color >> 24) & 0xFF;
                    
                    SDL_FRect rect;
                    rect.x = AX_FIXED_TO_FLOAT(cmd->draw_rect.position.x);
                    rect.y = AX_FIXED_TO_FLOAT(cmd->draw_rect.position.y);
                    rect.w = AX_FIXED_TO_FLOAT(cmd->draw_rect.size.x);
                    rect.h = AX_FIXED_TO_FLOAT(cmd->draw_rect.size.y);

                    SDL_SetRenderDrawColor(renderer, r, g, b, a);
                    SDL_RenderFillRect(renderer, &rect);
                }
            }
        }

        // Swap the buffers to display the frame
        SDL_RenderPresent(renderer);
    }

    // Clean up
    ax_engine_teardown();
    free(main_ram);
    free(frame_ram);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}