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
    cmd->clear.color = color;
    
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
    cmd->draw_rect.position = position;
    cmd->draw_rect.size = size;
    cmd->draw_rect.color = color;
    
    queue->count++;
    return AX_OK;
}