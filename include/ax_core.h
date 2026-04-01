#ifndef AXIOM_CORE_H
#define AXIOM_CORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @defgroup Core Memory & System Types
 * @brief Foundational engine types and zero-fragmentation memory management.
 * @{
 */

/**
 * @brief Standardized status return codes for the engine.
 */
typedef enum {
    AX_OK = 0,                   /**< Operation completed successfully. */
    AX_ERR_OUT_OF_MEMORY = 1,    /**< The memory arena has exhausted its capacity. */
    AX_ERR_INVALID_INPUT = 2,    /**< A provided parameter was NULL or out of bounds. */
    AX_ERR_UNALIGNED_ACCESS = 3,  /**< Requested memory alignment was not a power of 2. */
	// --- Asset & Graphics Errors ---
    AX_ERR_ASSET_LOAD_FAILED = 4, /**< Failed to locate, open, or decode an asset file. */
    AX_ERR_GPU_ALLOC_FAILED = 5,  /**< The GPU failed to allocate VRAM for a texture. */
    AX_ERR_GPU_UPLOAD_FAILED = 6  /**< Failed to transfer data across the bus to the GPU. */
} ax_result_t;

/**
 * @brief A linear memory allocator (Arena).
 * @details Allocates memory by advancing an offset. Freed entirely at once.
 */
typedef struct {
    uint8_t* buffer;      /**< Pointer to the start of the pre-allocated memory block. */
    size_t capacity;      /**< Total size of the block in bytes. */
    size_t offset;        /**< Current allocation head. */
} ax_arena_t;

/**
 * @brief Initializes an arena with a pre-existing memory buffer.
 * @param arena The arena to initialize.
 * @param backing_buffer The raw memory block to use. Must not be NULL.
 * @param capacity The size of the buffer in bytes. Must be > 0.
 * @return AX_OK on success, or AX_ERR_INVALID_INPUT on bad parameters.
 */
ax_result_t ax_arena_init(ax_arena_t* arena, void* backing_buffer, size_t capacity);

/**
 * @brief Allocates an aligned block of memory from the arena.
 * @param arena The arena to allocate from.
 * @param size The number of bytes requested.
 * @param alignment Must be a power of 2 (e.g., 4, 8, 16).
 * @param out_ptr Pointer to where the allocated address will be written.
 * @return AX_OK on success, or specific error code on failure.
 */
ax_result_t ax_arena_push(ax_arena_t* arena, size_t size, size_t alignment, void** out_ptr);

/**
 * @brief Instantly frees all memory in the arena by resetting the offset to zero.
 * @param arena The arena to clear.
 */
static inline void ax_arena_clear(ax_arena_t* arena) {
    arena->offset = 0;
}

/**
 * @brief Returns the amount of remaining memory in the arena.
 * @param arena The arena to check.
 * @return The number of bytes remaining.
 */
static inline size_t ax_arena_get_free_space(const ax_arena_t* arena) {
    return arena->capacity - arena->offset;
}

/**
 * @brief Allocates memory for a specific struct type from the arena.
 * @param arena_ptr Pointer to the ax_arena_t.
 * @param type The C struct/type to allocate (e.g., ax_vec2_t).
 * @param out_ptr Pointer to the destination variable.
 * @return AX_OK on success, or an error code.
 */
#define ax_push_struct(arena_ptr, type, out_ptr) \
    ax_arena_push((arena_ptr), sizeof(type), _Alignof(type), (void**)(out_ptr))

/**
 * @brief Allocates memory for an array of a specific type from the arena.
 * @param arena_ptr Pointer to the ax_arena_t.
 * @param type The C struct/type to allocate.
 * @param count The number of elements in the array.
 * @param out_ptr Pointer to the destination variable.
 * @return AX_OK on success, or an error code.
 */
#define ax_push_array(arena_ptr, type, count, out_ptr) \
    ax_arena_push((arena_ptr), sizeof(type) * (count), _Alignof(type), (void**)(out_ptr))

/** @} */

#endif // AXIOM_CORE_H