#include "ax_ui.h"
#include <stddef.h>

// Helper for fixed-point division to avoid missing math macro errors
static ax_fixed_t ax_fixed_div(ax_fixed_t a, ax_fixed_t b) {
    if (b == 0) return 0;
    return (ax_fixed_t)(((int64_t)a << 16) / b);
}

// -----------------------------------------------------------------------------
// UI Lifecycle
// -----------------------------------------------------------------------------

ax_result_t ax_ui_begin_frame(ax_ui_context_t* ctx, ax_arena_t* arena, ax_fixed_t screen_width, ax_fixed_t screen_height) {
    if (ctx == NULL || arena == NULL) return AX_ERR_INVALID_INPUT;

    ctx->arena = arena;
    
    // Allocate the root screen container
    ax_result_t res = ax_push_struct(arena, ax_ui_node_t, &ctx->root);
    if (res != AX_OK) return res;

    // The root node is hardcoded to fill the entire physical screen
    ctx->root->parent = NULL;
    ctx->root->first_child = NULL;
    ctx->root->last_child = NULL;
    ctx->root->next_sibling = NULL;
    
    ctx->root->computed_pos = (ax_vec2_t){ 0, 0 };
    ctx->root->computed_size = (ax_vec2_t){ screen_width, screen_height };
    
    ctx->root->layout_dir = AX_UI_DIR_COLUMN;
    ctx->root->bg_color = 0; // Transparent root

    return AX_OK;
}

ax_result_t ax_ui_push_node(ax_ui_context_t* ctx, ax_ui_node_t* parent, ax_ui_node_t** out_node) {
    if (ctx == NULL || out_node == NULL) return AX_ERR_INVALID_INPUT;
    
    if (parent == NULL) {
        parent = ctx->root; // Default attach to screen
    }

    ax_ui_node_t* node = NULL;
    ax_result_t res = ax_push_struct(ctx->arena, ax_ui_node_t, &node);
    if (res != AX_OK) {
        *out_node = NULL;
        return res;
    }

    // Zero-initialize fields
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->bg_color = 0;

    // Attach to the tree topology
    node->parent = parent;
    if (parent->last_child != NULL) {
        parent->last_child->next_sibling = node;
        parent->last_child = node;
    } else {
        parent->first_child = node;
        parent->last_child = node;
    }

    *out_node = node;
    return AX_OK;
}

// -----------------------------------------------------------------------------
// The Recursive Math Solver
// -----------------------------------------------------------------------------

static void solve_node_recursive(ax_ui_node_t* node) {
    if (node->first_child == NULL) return; // Leaf node, nothing to solve

    // Determine primary and cross axes based on flex direction
    int main_axis = (node->layout_dir == AX_UI_DIR_ROW) ? 0 : 1;
    int cross_axis = (node->layout_dir == AX_UI_DIR_ROW) ? 1 : 0;

    ax_fixed_t parent_main_size = (main_axis == 0) ? node->computed_size.x : node->computed_size.y;
    ax_fixed_t parent_cross_size = (cross_axis == 0) ? node->computed_size.x : node->computed_size.y;

    ax_fixed_t total_fixed_space = 0;
    ax_fixed_t total_flex_weight = 0;

    // Pass 1: Calculate rigid sizes and tally flex weights
    for (ax_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
        
        // --- Cross Axis (We assume 'Stretch to Fill' for the cross axis right now) ---
        ax_ui_size_t cross_rule = child->width[cross_axis];
        ax_fixed_t* child_cross_size = (cross_axis == 0) ? &child->computed_size.x : &child->computed_size.y;
        
        if (cross_rule.kind == AX_UI_SIZE_PIXELS) {
            *child_cross_size = cross_rule.value;
        } else if (cross_rule.kind == AX_UI_SIZE_PERCENT) {
            *child_cross_size = ax_math_mul(parent_cross_size, cross_rule.value);
        } else {
            *child_cross_size = parent_cross_size; // Flex stretches to fill cross axis
        }

        // --- Main Axis ---
        ax_ui_size_t main_rule = child->width[main_axis];
        ax_fixed_t* child_main_size = (main_axis == 0) ? &child->computed_size.x : &child->computed_size.y;

        if (main_rule.kind == AX_UI_SIZE_PIXELS) {
            *child_main_size = main_rule.value;
            total_fixed_space += main_rule.value;
        } else if (main_rule.kind == AX_UI_SIZE_PERCENT) {
            *child_main_size = ax_math_mul(parent_main_size, main_rule.value);
            total_fixed_space += *child_main_size;
        } else if (main_rule.kind == AX_UI_SIZE_FLEX) {
            total_flex_weight += main_rule.value; // Just tally the weight for Pass 2
        }
    }

    // Pass 2: Distribute remaining space to Flex children
    ax_fixed_t free_space = parent_main_size - total_fixed_space;
    if (free_space < 0) free_space = 0; // Prevent negative sizes if elements over-constrain

    if (total_flex_weight > 0) {
        for (ax_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
            ax_ui_size_t main_rule = child->width[main_axis];
            if (main_rule.kind == AX_UI_SIZE_FLEX) {
                ax_fixed_t* child_main_size = (main_axis == 0) ? &child->computed_size.x : &child->computed_size.y;
                ax_fixed_t weight_ratio = ax_fixed_div(main_rule.value, total_flex_weight);
                *child_main_size = ax_math_mul(free_space, weight_ratio);
            }
        }
    }

    // Pass 3: Sequence positions chronologically and recurse
    ax_fixed_t current_main_pos = (main_axis == 0) ? node->computed_pos.x : node->computed_pos.y;
    ax_fixed_t cross_pos = (cross_axis == 0) ? node->computed_pos.x : node->computed_pos.y;

    for (ax_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
        ax_fixed_t* child_main_pos = (main_axis == 0) ? &child->computed_pos.x : &child->computed_pos.y;
        ax_fixed_t* child_cross_pos = (cross_axis == 0) ? &child->computed_pos.x : &child->computed_pos.y;

        *child_main_pos = current_main_pos;
        *child_cross_pos = cross_pos;

        ax_fixed_t child_main_size = (main_axis == 0) ? child->computed_size.x : child->computed_size.y;
        current_main_pos += child_main_size; // Advance the cursor for the next sibling

        // Depth-first resolution: Now that this child knows its final size, solve its children
        solve_node_recursive(child);
    }
}

ax_result_t ax_ui_solve_layout(ax_ui_context_t* ctx) {
    if (ctx == NULL || ctx->root == NULL) return AX_ERR_INVALID_INPUT;
    solve_node_recursive(ctx->root);
    return AX_OK;
}

// -----------------------------------------------------------------------------
// UI Renderer
// -----------------------------------------------------------------------------

static void draw_node_recursive(ax_ui_node_t* node, ax_render_queue_t* queue) {
    if (node->bg_color != 0) { 
        ax_graphics_push_rect(queue, node->computed_pos, node->computed_size, node->bg_color);
    }
    for (ax_ui_node_t* child = node->first_child; child != NULL; child = child->next_sibling) {
        draw_node_recursive(child, queue);
    }
}

ax_result_t ax_ui_draw(const ax_ui_context_t* ctx, ax_render_queue_t* render_queue) {
    if (ctx == NULL || ctx->root == NULL || render_queue == NULL) return AX_ERR_INVALID_INPUT;
    draw_node_recursive(ctx->root, render_queue);
    return AX_OK;
}