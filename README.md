# Axiom Engine

A strictly decoupled, deterministic, and zero-allocation 2D game engine built entirely in C.

Axiom is designed around strict systems-programming principles. The core engine logic is completely isolated from the host Operating System, ensuring that the simulation runs identically across different hardware. By leveraging custom memory arenas, fixed-point mathematics, and a data-driven render queue, Axiom provides a stable, cross-platform foundation for 2D games and applications.

## Core Architecture

* **Zero-Fragmentation Memory (Arenas):** The engine avoids runtime `malloc`/`free` calls during gameplay. Permanent state lives in a pre-allocated main arena, while per-frame data (like UI nodes and raw pixel decoding) utilizes a temporary `frame_arena` that is cleared in O(1) time at the end of every tick, guaranteeing zero memory leaks.
* **Deterministic Simulation:** To ensure identical behavior on ARM (Android) and x86 (Windows) processors, Axiom uses a custom **16.16 Fixed-Point Integer** math library (`ax_math.h`). The physics simulation is strictly integer-based.
* **Data-Driven Rendering:** The core engine never makes direct graphics API calls. Instead, `ax_engine_tick` outputs an `ax_render_queue_t` containing an array of tagged-union rendering commands. These commands share an exact memory footprint, allowing them to be tightly packed for maximum CPU cache efficiency.
* **Zero-Leak Asset Registry:** Assets are read from the OS into the temporary frame arena, decoded via `stb_image`, uploaded to GPU VRAM, and then immediately flushed from CPU memory, keeping the engine's permanent memory footprint exceptionally small.
* **Immediate-Mode Flex UI:** Features a custom UI solver that allows for hierarchical, percentage-based, and flex-weight layout construction.
* **Platform Abstraction Layer (PAL):** The OS boundary is strictly defined via a function pointer contract. The host platform (currently powered by SDL3) is "dumb"—it only handles windowing, raw hardware input, and flushing the render queue to the screen.

## Directory Structure

* `include/` - Core engine API headers (`ax_core.h`, `ax_math.h`, `ax_graphics.h`, etc.).
* `src/core/` - The core engine singleton, memory arenas, and the deterministic game loop.
* `src/platform/` - The OS boundary layer. Contains the SDL3 implementation (`sdl_main.c`) and the headless test runner (`headless_main.c`).
* `src/graphics/` - The render command queue and data payload structures.
* `src/ui/` - The immediate-mode flex-box layout solver.
* `src/assets/` - The STB-powered asset decoder and memory router.
* `android/` - The native Android Studio / Gradle project structure.
* `vendor/` - Third-party dependencies (SDL3, stb_image).

## Supported Platforms

* **Windows:** Compiles via CMake / MSBuild.
* **Android:** Compiles via Gradle / CMake native build (packaged as an APK).
* **Headless:** A pure-console target for running deterministic tests and continuous integration without a graphics driver.

## Building the Engine

### Windows (CMake)
```bash
# Generate the build files
cmake -B build

# Build the executable
cmake --build build
