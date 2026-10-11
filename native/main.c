#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gba_types.h"
#include "gba_bridge.h"
#include "platform_macos.h"

// 60 FPS Real-Time Game Loop Frame Callback
static void on_engine_frame(void* user_data, u32* framebuffer, u16 key_state, bool skip_render) {
    (void)user_data;
    // Execute authentic ARM7TDMI machine code with real-time catch-up capability
    gba_bridge_step_ex(framebuffer, key_state, skip_render);
}

int main(int argc, char** argv) {
    printf("========================================================\n");
    printf("  THE LEGEND OF ZELDA: THE MINISH CAP - NATIVE ENGINE   \n");
    printf("  100%% Authentic Capcom Game Loop & Assets [USA: BZME]  \n");
    printf("========================================================\n");

    const char* rom_path = "Legend of Zelda, The - The Minish Cap (USA).gba";
    if (argc > 1) {
        rom_path = argv[1];
    }

    if (!gba_bridge_init(rom_path, "gba_core/resources/bios.bin")) {
        fprintf(stderr, "[Fatal] Failed to initialize GBA execution bridge for %s\n", rom_path);
        return 1;
    }

    PlatformConfig config = {
        .on_frame = on_engine_frame,
        .user_data = NULL,
        .window_title = "The Legend of Zelda: The Minish Cap",
        .window_width = GBA_SCREEN_WIDTH * 3,   // 720px
        .window_height = GBA_SCREEN_HEIGHT * 3  // 480px
    };

    int result = platform_macos_run(&config);

    gba_bridge_free();
    return result;
}
