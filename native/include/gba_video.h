#ifndef GBA_VIDEO_H
#define GBA_VIDEO_H

#include "gba_types.h"
#include "gba_mmu.h"

typedef struct {
    u32 framebuffer[GBA_PIXEL_COUNT]; // 240x160 32-bit RGBA pixels
    u16 dispcnt;                       // Display control register (0x04000000)
    u16 bgcnt[4];                      // Background control registers
    u16 bghofs[4];                     // BG horizontal scroll
    u16 bgvofs[4];                     // BG vertical scroll
    u16 vcount;                        // Current scanline (0 - 227)
    bool in_vblank;
} GbaVideo;

void video_init(GbaVideo* video);
void video_render_scanline(GbaVideo* video, GbaMMU* mmu, u16 line);
void video_render_frame(GbaVideo* video, GbaMMU* mmu);
void video_clear(GbaVideo* video, u32 color);

// Helper to decode a 4bpp tile into an 8x8 RGBA pixel buffer
void video_decode_tile_4bpp(const u8* tile_data, const u16* palette, u32* out_pixels, u32 stride, bool flip_x, bool flip_y);

#endif // GBA_VIDEO_H

