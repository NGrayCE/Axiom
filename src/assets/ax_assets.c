#include "ax_assets.h"
#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#include "stb_image.h"

ax_result_t ax_asset_load_image(ax_arena_t* arena, const uint8_t* file_data, size_t file_size, ax_image_t* out_image) {
    if (!arena || !file_data || file_size == 0 || !out_image) {
        return AX_ERR_INVALID_INPUT;
    }

    int temp_width = 0, temp_height = 0, temp_channels = 0;
    int desired_channels = 4;
    
    // We are now decoding purely from RAM. No hard drive access!
    uint8_t* temp_pixels = stbi_load_from_memory(file_data, (int)file_size, &temp_width, &temp_height, &temp_channels, desired_channels);
    
    if (!temp_pixels) {
        return AX_ERR_ASSET_LOAD_FAILED; 
    }
	
	uint32_t safe_width = (uint32_t)temp_width;
    uint32_t safe_height = (uint32_t)temp_height;
    uint32_t safe_channels = (uint32_t)desired_channels;
    uint32_t image_size = safe_width * safe_height * safe_channels;
	
	// 3. Attempt to secure permanent engine memory
    uint8_t* perm_pixels = NULL;
    
    // Safely request an array of bytes from the arena
    ax_result_t alloc_res = ax_push_array(arena, uint8_t, image_size, &perm_pixels);
    
    if (alloc_res != AX_OK || perm_pixels == NULL) {
        // The file loaded, but our engine is out of memory! Prevent the STB leak.
        stbi_image_free(temp_pixels);
        return AX_ERR_OUT_OF_MEMORY;
    }
    // 4. Commit data
    memcpy(perm_pixels, temp_pixels, image_size);

    out_image->width = safe_width;
    out_image->height = safe_height;
    out_image->channels = safe_channels;
    out_image->pixels = perm_pixels;

    stbi_image_free(temp_pixels);

    return AX_OK;
}