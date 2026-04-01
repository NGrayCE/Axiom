#pragma once
#include "ax_core.h"
#include <stdint.h>

/**
 * @struct ax_image_t
 * @brief A raw block of pixel data living in a memory arena.
 */
typedef struct {
    uint32_t width;     /**< The width of the image in pixels. */
    uint32_t height;    /**< The height of the image in pixels. */
    uint32_t channels;  /**< The number of color channels (usually 4 for RGBA). */
    uint8_t* pixels;    /**< Pointer to the raw byte data. */
} ax_image_t;

/**
 * @brief Loads a PNG or BMP image from disk into the provided memory arena.
 * * @param arena A pointer to the memory arena to allocate the pixel data from.
 * @param filepath The null-terminated string representing the path to the image file.
 * @param out_image A pointer to an ax_image_t struct to populate with the loaded data.
 * @return ax_result_t AX_SUCCESS if the image was successfully loaded and copied, AX_FAILURE otherwise.
 */
ax_result_t ax_asset_load_image(ax_arena_t* arena, const char* filepath, ax_image_t* out_image);