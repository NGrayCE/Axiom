#ifndef AXIOM_UI_H
#define AXIOM_UI_H

#include "ax_core.h"
#include "ax_math.h"
#include "ax_graphics.h"

/**
 * @defgroup UI Constraint Solver
 * @brief Immediate-mode, flex-box style hierarchical UI layout engine.
 * @{
 */

/**
 * @brief Defines how a node calculates its size on a specific axis.
 */
typedef enum {
    AX_UI_SIZE_PIXELS = 0, /**< Strict fixed-point pixel dimension. */
    AX_UI_SIZE_PERCENT,    /**< Percentage of the parent's computed size (0.0 to 1.0). */
    AX_UI_SIZE_FLEX        /**< Proportion of the parent's remaining free space. */
} ax_ui_size_kind_t;

/**
 * @brief A generic sizing constraint.
 */
typedef struct {
    ax_ui_size_kind_t kind;
    ax_fixed_t value;       /**< Fixed-point representation of the pixels, percent, or flex weight. */
} ax_ui_size_t;

/** @brief Helper macros for clean UI definitions */
#define AX_SIZE_PX(pixels)  (ax_ui_size_t){ AX_UI_SIZE_PIXELS, AX_INT_TO_FIXED(pixels) }
#define AX_SIZE_PCT(float_pct) (ax_ui_size_t){ AX_UI_SIZE_PERCENT, AX_FLOAT_TO_FIXED(float_pct) }
#define AX_SIZE_FLEX(weight) (ax_ui_size_t){ AX_UI_SIZE_FLEX, AX_INT_TO_FIXED(weight) }

/**
 * @brief Defines the packing direction for child nodes.
 */
typedef enum {
    AX_UI_DIR_COLUMN = 0, /**< Children stack top-to-bottom. */
    AX_UI_DIR_ROW         /**< Children stack left-to-right. */
} ax_ui_dir_t;

/**
 * @brief A single node in the UI hierarchy tree.
 */
typedef struct ax_ui_node_t ax_ui_node_t;
struct ax_ui_node_t {
    // Tree Topology
    ax_ui_node_t* parent;
    ax_ui_node_t* first_child;
    ax_ui_node_t* last_child;
    ax_ui_node_t* next_sibling;

    // Input Constraints (Set by the developer)
    ax_ui_size_t width[2];  // width[0] is X axis, width[1] is Y axis (height)
    ax_ui_dir_t layout_dir;
    ax_color_t bg_color;

    // Output Data (Calculated by the engine)
    ax_vec2_t computed_pos;
    ax_vec2_t computed_size;
};

/**
 * @brief Global UI context for the current frame.
 */
typedef struct {
    ax_arena_t* arena;    /**< The frame scratchpad to allocate nodes from. */
    ax_ui_node_t* root;   /**< The top-level screen container. */
} ax_ui_context_t;

/**
 * @brief Initializes the UI context for a new frame.
 * @param ctx The context to initialize.
 * @param arena The scratchpad arena.
 * @param screen_width The current width of the host window.
 * @param screen_height The current height of the host window.
 * @return AX_OK on success.
 */
ax_result_t ax_ui_begin_frame(ax_ui_context_t* ctx, ax_arena_t* arena, ax_fixed_t screen_width, ax_fixed_t screen_height);

/**
 * @brief Creates a new UI node and attaches it to a parent.
 * @param ctx The active UI context.
 * @param parent The parent node (if NULL, attaches to root).
 * @param out_node Pointer to where the allocated node address will be written.
 * @return AX_OK on success, or AX_ERR_OUT_OF_MEMORY.
 */
ax_result_t ax_ui_push_node(ax_ui_context_t* ctx, ax_ui_node_t* parent, ax_ui_node_t** out_node);

/**
 * @brief Runs the mathematical constraint solver over the entire tree.
 * @param ctx The active UI context.
 * @return AX_OK on success.
 */
ax_result_t ax_ui_solve_layout(ax_ui_context_t* ctx);

/**
 * @brief Translates the computed UI tree into draw commands.
 * @param ctx The active UI context.
 * @param render_queue The graphics queue to push commands into.
 * @return AX_OK on success.
 */
ax_result_t ax_ui_draw(const ax_ui_context_t* ctx, ax_render_queue_t* render_queue);

/** @} */

#endif // AXIOM_UI_H