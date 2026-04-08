#include "ax_graphics.h"
#include <stddef.h>

// -----------------------------------------------------------------------------
// Queue Initialization
// -----------------------------------------------------------------------------
ax_result_t ax_graphics_queue_create(ax_arena_t* arena, uint32_t max_commands, ax_render_queue_t** out_queue) {
    if (arena == NULL || max_commands == 0 || out_queue == NULL) {
        if (out_queue) *out_queue = NULL;
        return AX_ERR_INVALID_INPUT;
    }

    // 1. Allocate the queue wrapper struct
    ax_render_queue_t* queue = NULL;
    ax_result_t res_queue = ax_push_struct(arena, ax_render_queue_t, &queue);
    if (res_queue != AX_OK) {
        *out_queue = NULL;
        return res_queue;
    }

    // 2. Allocate the contiguous array of commands
    ax_render_cmd_t* commands = NULL;
    ax_result_t res_cmds = ax_push_array(arena, ax_render_cmd_t, max_commands, &commands);
    if (res_cmds != AX_OK) {
        *out_queue = NULL;
        return res_cmds;
    }

    // 3. Initialize the state
    queue->commands = commands;
    queue->capacity = max_commands;
    queue->count = 0;

    *out_queue = queue;
    return AX_OK;
}

// -----------------------------------------------------------------------------
// Command Push Functions
// -----------------------------------------------------------------------------

ax_result_t ax_graphics_push_clear(ax_render_queue_t* queue, ax_color_t color) {
    if (queue == NULL) return AX_ERR_INVALID_INPUT;
    
    // Bounds check to prevent buffer overflow
    if (queue->count >= queue->capacity) {
        return AX_ERR_OUT_OF_MEMORY;
    }

    // Grab the next available slot in the array
    ax_render_cmd_t* cmd = &queue->commands[queue->count];
    
    // Populate the tagged union
    cmd->type = AX_RENDER_CMD_CLEAR;
    cmd->as.clear.color = color;
    
    queue->count++;
    return AX_OK;
}

ax_result_t ax_graphics_push_rect(ax_render_queue_t* queue, ax_vec2_t position, ax_vec2_t size, ax_color_t color) {
    if (queue == NULL) return AX_ERR_INVALID_INPUT;
    
    if (queue->count >= queue->capacity) {
        return AX_ERR_OUT_OF_MEMORY;
    }

    ax_render_cmd_t* cmd = &queue->commands[queue->count];
    
    cmd->type = AX_RENDER_CMD_DRAW_RECT;
    cmd->as.draw_rect.position = position;
    cmd->as.draw_rect.size = size;
    cmd->as.draw_rect.color = color;
    
    queue->count++;
    return AX_OK;
}

ax_result_t ax_graphics_push_texture(ax_render_queue_t* queue, const ax_texture_t* tex, 
                              ax_vec2_t dst_pos, ax_vec2_t dst_size,
                              ax_vec2_t src_pos, ax_vec2_t src_size) {
    if (queue->count >= queue->capacity) {
        return AX_ERR_OUT_OF_MEMORY;
    }
    
    ax_render_cmd_t* cmd = &queue->commands[queue->count++];
    cmd->type = AX_RENDER_CMD_DRAW_TEXTURE;
    cmd->as.draw_texture.texture = tex;
    cmd->as.draw_texture.dst_pos = dst_pos;
    cmd->as.draw_texture.dst_size = dst_size;
    cmd->as.draw_texture.src_pos = src_pos;
    cmd->as.draw_texture.src_size = src_size;
}

void ax_graphics_push_text(ax_render_queue_t* queue, const ax_texture_t* font_atlas, 
                           const char* text, ax_vec2_t start_pos, 
                           int glyph_w, int glyph_h) {
    if (!font_atlas || !text) return;

    ax_vec2_t current_pos = start_pos;
    ax_vec2_t dst_size = { AX_INT_TO_FIXED(glyph_w), AX_INT_TO_FIXED(glyph_h) };
    ax_vec2_t src_size = { AX_INT_TO_FIXED(glyph_w), AX_INT_TO_FIXED(glyph_h) };

    int columns = 16; 

    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = text[i];

        // Handle line breaks
        if (c == '\n') {
            current_pos.x = start_pos.x;
            current_pos.y += dst_size.y; // Move down one line
            continue;
        }

        // Math: Find the exact grid coordinate for this ASCII character
        int col = c % columns;
        int row = c / columns;

        ax_vec2_t src_pos = { 
            AX_INT_TO_FIXED(col * glyph_w), 
            AX_INT_TO_FIXED(row * glyph_h) 
        };

        // Push the perfectly sliced letter to the render queue
        ax_graphics_push_texture(queue, font_atlas, current_pos, dst_size, src_pos, src_size);

        // Move the "cursor" to the right for the next letter
        current_pos.x += dst_size.x;
    }
}
