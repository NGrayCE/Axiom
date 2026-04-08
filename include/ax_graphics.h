#ifndef AXIOM_GRAPHICS_H
#define AXIOM_GRAPHICS_H

#include "ax_core.h"
#include "ax_math.h"
#include "ax_assets.h"

/**
 * @defgroup Graphics Render Command Queue
 * @brief Data-driven rendering instructions decoupled from the host OS API.
 * @{
 */

/**
 * @brief 32-bit RGBA color representation.
 * Memory layout: 0xAABBGGRR (Little Endian).
 */
typedef uint32_t ax_color_t;

/** @brief Helper macro to pack 8-bit RGBA channels into an ax_color_t. */
#define AX_COLOR_MAKE(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | ((uint32_t)(r)))

// Standard built-in colors
#define AX_COLOR_BLACK   AX_COLOR_MAKE(0, 0, 0, 255)
#define AX_COLOR_WHITE   AX_COLOR_MAKE(255, 255, 255, 255)
#define AX_COLOR_RED     AX_COLOR_MAKE(255, 0, 0, 255)
#define AX_COLOR_BLUE    AX_COLOR_MAKE(0, 0, 255, 255)

/**
 * @brief Identifies the type of render command.
 */
typedef enum {
    AX_RENDER_CMD_CLEAR = 0,       /**< Clears the entire screen to a specific color. */
    AX_RENDER_CMD_DRAW_RECT = 1,   /**< Draws a filled 2D rectangle. */
    AX_RENDER_CMD_DRAW_TEXTURE = 2 /**< Command to draw a 2D texture to the screen. */
} ax_render_cmd_type_t;

/**
 * @brief Command payload: Clear the screen.
 */
typedef struct {
    ax_color_t color;
} ax_cmd_clear_t;

/**
 * @brief Command payload: Draw a filled rectangle.
 */
typedef struct {
    ax_vec2_t position; /**< Fixed-point Top-Left coordinate. */
    ax_vec2_t size;     /**< Fixed-point Width and Height. */
    ax_color_t color;
} ax_cmd_draw_rect_t;

/**
 * @struct ax_texture_t
 * @brief The opaque handle that represents an image living on the GPU VRAM.
 */
typedef struct {
    uint32_t width;         /**< The width of the texture in pixels. */
    uint32_t height;        /**< The height of the texture in pixels. */
    void* platform_handle;  /**< The internal platform-specific API handle (e.g., SDL_Texture*). */
} ax_texture_t;

/**
 * @struct ax_cmd_draw_texture_t
 * @brief Command payload: Draw a 2D texture.
 */
typedef struct {
    const ax_texture_t* texture;
			ax_vec2_t dst_pos;   // Where it goes on the screen
			ax_vec2_t dst_size;  // How big it is on the screen
			ax_vec2_t src_pos;   // NEW: Top-left pixel of the slice
			ax_vec2_t src_size;  // NEW: Width/Height of the slice
} ax_cmd_draw_texture_t;

/**
 * @brief A single, polymorphic render instruction.
 * @details Uses a tagged union to ensure all commands consume the exact same memory footprint, 
 * allowing them to be tightly packed into an array to maximize CPU cache hits.
 */
typedef struct {
    ax_render_cmd_type_t type;
    union {
        ax_cmd_clear_t clear;
        ax_cmd_draw_rect_t draw_rect;
        ax_cmd_draw_texture_t draw_texture;
    } as;
} ax_render_cmd_t;

/**
 * @brief The complete queue of graphics instructions for a single frame.
 */
typedef struct {
    ax_render_cmd_t* commands; /**< Pointer to the array of commands. */
    uint32_t count;            /**< Number of active commands in the queue. */
    uint32_t capacity;         /**< Maximum number of commands the array can hold. */
} ax_render_queue_t;

/**
 * @brief Initializes a render queue by allocating memory from the provided arena.
 * @param arena The scratchpad arena to allocate the command array from.
 * @param max_commands The maximum number of commands this queue can hold.
 * @param out_queue Pointer to where the initialized queue struct will be written.
 * @return AX_OK on success, or AX_ERR_OUT_OF_MEMORY.
 */
ax_result_t ax_graphics_queue_create(ax_arena_t* arena, uint32_t max_commands, ax_render_queue_t** out_queue);

/**
 * @brief Pushes a clear screen command into the queue.
 * @param queue The queue to push to.
 * @param color The background color.
 * @return AX_OK on success, or AX_ERR_OUT_OF_MEMORY if the queue is full.
 */
ax_result_t ax_graphics_push_clear(ax_render_queue_t* queue, ax_color_t color);

/**
 * @brief Pushes a rectangle draw command into the queue.
 * @param queue The queue to push to.
 * @param position Top-Left coordinate (Fixed-point).
 * @param size Width and Height (Fixed-point).
 * @param color The fill color.
 * @return AX_OK on success, or AX_ERR_OUT_OF_MEMORY if the queue is full.
 */
ax_result_t ax_graphics_push_rect(ax_render_queue_t* queue, ax_vec2_t position, ax_vec2_t size, ax_color_t color);

/**
 * @brief Pushes a texture draw command to the render queue.
 * @param queue A pointer to the active render queue.
 * @param texture A constant pointer to the texture to draw.
 * @param position The X/Y screen coordinates.
 * @param size The physical width and height to draw the texture.
 * @return ax_result_t AX_OK on success, or AX_ERR_OUT_OF_MEMORY if the queue is full.
 */
ax_result_t ax_graphics_push_texture(ax_render_queue_t* queue, const ax_texture_t* tex, 
                              ax_vec2_t dst_pos, ax_vec2_t dst_size,
                              ax_vec2_t src_pos, ax_vec2_t src_size);

/** @} */

#endif // AXIOM_GRAPHICS_H