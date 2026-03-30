#ifndef AXIOM_PLATFORM_H
#define AXIOM_PLATFORM_H

#include "ax_core.h"
#include "ax_math.h"

/**
 * @defgroup Platform Platform Abstraction Layer (PAL)
 * @brief The strict boundary between the deterministic engine and the host OS.
 * @{
 */
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
    ax_result_t (*read_asset)(const char* filename, void** out_buffer, size_t* out_size);
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
 * @brief Advances the engine simulation by one deterministic tick.
 * @param input_queue The chronological queue of events that occurred since the last tick.
 * @return AX_OK if the frame processed correctly.
 */
ax_result_t ax_engine_tick(const ax_input_queue_t* input_queue);

/** @} */

#endif // AXIOM_PLATFORM_H