#include "ax_core.h"

ax_result_t ax_arena_init(ax_arena_t* arena, void* backing_buffer, size_t capacity) {
    // Validate inputs to prevent silent catastrophic failures later
    if (arena == NULL || backing_buffer == NULL || capacity == 0) {
        return AX_ERR_INVALID_INPUT;
    }

    arena->buffer = (uint8_t*)backing_buffer;
    arena->capacity = capacity;
    arena->offset = 0;

    return AX_OK;
}

ax_result_t ax_arena_push(ax_arena_t* arena, size_t size, size_t alignment, void** out_ptr) {
    if (out_ptr == NULL) {
        return AX_ERR_INVALID_INPUT;
    }
    
    // Default the output to NULL so the caller doesn't use a dangling pointer on failure
    *out_ptr = NULL; 

    // 1. Ensure the alignment is a power of 2.
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return AX_ERR_UNALIGNED_ACCESS;
    }

    // 2. Calculate the current unaligned memory address.
    uintptr_t current_ptr = (uintptr_t)(arena->buffer + arena->offset);

    // 3. Calculate padding
    size_t padding = 0;
    uintptr_t modulo = current_ptr & (alignment - 1);
    if (modulo != 0) {
        padding = alignment - modulo;
    }

    // 4. Check for Out of Memory (OOM)
    if (arena->offset + padding + size > arena->capacity) {
        return AX_ERR_OUT_OF_MEMORY;
    }

    // 5. Commit the allocation
    arena->offset += padding;          
    *out_ptr = arena->buffer + arena->offset; 
    arena->offset += size;             

    return AX_OK;
}