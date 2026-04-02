#ifndef AXIOM_PLATFORM_H
#define AXIOM_PLATFORM_H

#include "ax_core.h"
#include "ax_math.h"
#include "ax_graphics.h"
#include "ax_assets.h"

/**
 * @defgroup Platform Platform Abstraction Layer (PAL)
 * @brief The strict boundary between the deterministic engine and the host OS.
 * @{
 */
 
 /**
 * @brief The read-only state of the game simulation.
 */
typedef struct {
    ax_vec2_t position;
    ax_vec2_t velocity;
    ax_vec2_t bounds;
    uint32_t frame_count;
} ax_game_state_t;

/**
 * @brief Retrieves a read-only pointer to the current game state.
 * @param out_state Pointer to where the read-only state address will be written.
 * @return AX_OK on success, or AX_ERR_INVALID_INPUT if the engine is not booted or the pointer is NULL.
 */
ax_result_t ax_engine_get_state(const ax_game_state_t** out_state);

/**
 * @brief Categorizes the type of hardware input event.
 */
typedef enum {
    AX_INPUT_NONE = 0,
    AX_INPUT_TOUCH_DOWN,
    AX_INPUT_TOUCH_UP,
    AX_INPUT_TOUCH_MOVE
} ax_input_type_t;

/**
 * @brief A single, discrete input event.
 */
typedef struct {
    ax_input_type_t type;
    ax_vec2_t position;      /**< Fixed-point coordinates of the event. */
    uint32_t timestamp_us;   /**< Microsecond timestamp of when the OS caught the hardware interrupt. */
} ax_input_event_t;

/** @brief Maximum number of events the OS can queue per frame to prevent buffer overflow. */
#define AX_MAX_INPUT_EVENTS_PER_FRAME 16

/**
 * @brief The complete queue of input events accumulated since the last engine tick.
 */
typedef struct {
    ax_input_event_t events[AX_MAX_INPUT_EVENTS_PER_FRAME];
    uint32_t count;
} ax_input_queue_t;

/**
 * @brief The System API provided by the host Operating System.
 * @details The engine core cannot call OS libraries directly. The host OS 
 * must populate this struct with function pointers and provide it at boot.
 */
typedef struct {
    /**
     * @brief Retrieves the current high-resolution system time.
     * @return Monotonic time in microseconds.
     */
    uint64_t (*get_ticks_us)(void);

    /**
     * @brief Outputs a debug message to the platform's native console.
     * @param message Null-terminated C string to log.
     */
    void (*log_message)(const char* message);

    /**
     * @brief Reads a binary file from the platform's asset storage.
     * @param filename The path to the asset.
     * @param out_buffer Pointer where the loaded memory address will be written.
     * @param out_size Pointer where the file size will be written.
     * @return AX_OK on success, or an error code.
     */
	ax_result_t (*read_asset)(const char* filename, ax_arena_t* arena, void** out_buffer, size_t* out_size);
} ax_system_api_t;

/**
 * @brief Initializes the engine core and its memory arenas.
 * @param api The struct of OS-provided function pointers.
 * @param main_memory The pre-allocated RAM block for permanent state.
 * @param main_size The size of the permanent RAM block.
 * @param frame_memory The pre-allocated RAM block for the per-frame scratchpad.
 * @param frame_size The size of the per-frame scratchpad.
 * @return AX_OK on successful boot.
 */
ax_result_t ax_engine_boot(const ax_system_api_t* api, 
                           void* main_memory, size_t main_size,
                           void* frame_memory, size_t frame_size);
						   
/**
 * @brief Shuts down the engine and resets its initialized state.
 * @details Useful for clean reboots during testing or OS application lifecycle events.
 * @return AX_OK on successful teardown.
 */
ax_result_t ax_engine_teardown(void);

/**
 * @brief Advances the engine simulation by one deterministic tick.
 * @param input_queue The chronological queue of events that occurred since the last tick.
 * @param screen_width The device's screen width in pixels
 * @param screen_height The device's screen height in pixels
 * @param out_render_queue Pointer to where the engine will write the frame's render commands.
 * @return AX_OK if the frame processed correctly.
 */
ax_result_t ax_engine_tick(const ax_input_queue_t* input_queue, 
                           ax_fixed_t screen_width, 
                           ax_fixed_t screen_height, 
                           ax_render_queue_t** out_render_queue);

/**
 * @brief Uploads raw CPU pixel data across the bus to the GPU VRAM.
 * * @param image A constant pointer to the parsed CPU image data.
 * @param out_texture A pointer to an ax_texture_t struct to populate with the GPU handle.
 * @return ax_result_t AX_SUCCESS if the texture was allocated and uploaded, AX_FAILURE otherwise.
 */
ax_result_t ax_platform_upload_texture(const ax_image_t* image, ax_texture_t* out_texture);

/**
 * @brief Destroys a GPU texture, freeing its VRAM.
 * * @param texture A pointer to the texture handle to destroy.
 * @return ax_result_t AX_SUCCESS upon successful destruction.
 */
ax_result_t ax_platform_destroy_texture(ax_texture_t* texture);
/** @} */

#endif // AXIOM_PLATFORM_H