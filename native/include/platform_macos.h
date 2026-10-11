#ifndef PLATFORM_MACOS_H
#define PLATFORM_MACOS_H

#include "gba_types.h"
#include "gba_keypad.h"
#include <stdbool.h>

typedef struct {
    void (*on_frame)(void* user_data, u32* framebuffer, u16 key_state, bool skip_render);
    void* user_data;
    const char* window_title;
    int window_width;
    int window_height;
} PlatformConfig;

int platform_macos_run(const PlatformConfig* config);

#endif // PLATFORM_MACOS_H
