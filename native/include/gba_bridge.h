#ifndef GBA_BRIDGE_H
#define GBA_BRIDGE_H

#include "gba_types.h"
#include <stdbool.h>

// Initialize the native GBA execution bridge with the authentic ROM
bool gba_bridge_init(const char* rom_path, const char* bios_path);

// Run 1 frame (VBlank) of authentic GBA bytecode, updating keypad and extracting pixels
void gba_bridge_step(u32* out_pixels, u16 key_state);

// Step 1 frame with option to skip scanline rasterization for real-time catch-up
void gba_bridge_step_ex(u32* out_pixels, u16 key_state, bool skip_render);

// Save battery progress to disk and cleanup
void gba_bridge_free(void);

#endif // GBA_BRIDGE_H
