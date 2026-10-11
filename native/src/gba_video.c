#include "gba_video.h"
#include <string.h>

void video_init(GbaVideo* video) {
    memset(video, 0, sizeof(GbaVideo));
    video_clear(video, 0xFF000000); // Black screen default
}

void video_clear(GbaVideo* video, u32 color) {
    for (int i = 0; i < GBA_PIXEL_COUNT; i++) {
        video->framebuffer[i] = color;
    }
}

void video_decode_tile_4bpp(const u8* tile_data, const u16* palette, u32* out_pixels, u32 stride, bool flip_x, bool flip_y) {
    if (!tile_data || !palette || !out_pixels) return;

    for (int y = 0; y < 8; y++) {
        int py = flip_y ? (7 - y) : y;
        for (int x = 0; x < 8; x += 2) {
            u8 byte = tile_data[y * 4 + (x / 2)];
            u8 c0 = byte & 0x0F;
            u8 c1 = (byte >> 4) & 0x0F;

            int px0 = flip_x ? (7 - x) : x;
            int px1 = flip_x ? (6 - x) : (x + 1);

            if (c0 != 0) {
                out_pixels[py * stride + px0] = BGR555_TO_RGBA32(palette[c0]);
            }
            if (c1 != 0) {
                out_pixels[py * stride + px1] = BGR555_TO_RGBA32(palette[c1]);
            }
        }
    }
}

void video_render_scanline(GbaVideo* video, GbaMMU* mmu, u16 line) {
    if (line >= GBA_SCREEN_HEIGHT) return;

    u32* scanline = &video->framebuffer[line * GBA_SCREEN_WIDTH];
    u16* bg_palette = (u16*)mmu->pram;
    u16 backdrop = bg_palette[0];
    u32 backdrop_rgba = BGR555_TO_RGBA32(backdrop);

    // Initialize line with backdrop color
    for (int x = 0; x < GBA_SCREEN_WIDTH; x++) {
        scanline[x] = backdrop_rgba;
    }

    // Render Background Layers (Mode 0: Text Mode)
    for (int bg = 3; bg >= 0; bg--) {
        u16 cnt = video->bgcnt[bg];
        u32 char_base = ((cnt >> 2) & 0x03) * 0x4000;
        u32 screen_base = ((cnt >> 8) & 0x1F) * 0x0800;

        int hscroll = video->bghofs[bg] & 0x1FF;
        int vscroll = video->bgvofs[bg] & 0x1FF;
        int fine_y = (line + vscroll) & 7;
        int tile_y = ((line + vscroll) >> 3) & 31;

        u16* map = (u16*)(mmu->vram + screen_base);
        u8* tiles = mmu->vram + char_base;

        for (int x = 0; x < GBA_SCREEN_WIDTH; x++) {
            int fine_x = (x + hscroll) & 7;
            int tile_x = ((x + hscroll) >> 3) & 31;

            u16 tile_entry = map[tile_y * 32 + tile_x];
            u16 tile_num = tile_entry & 0x3FF;
            bool flip_h = (tile_entry & (1 << 10)) != 0;
            bool flip_v = (tile_entry & (1 << 11)) != 0;
            u8 pal_bank = (tile_entry >> 12) & 0x0F;

            int ty = flip_v ? (7 - fine_y) : fine_y;
            int tx = flip_h ? (7 - fine_x) : fine_x;

            u8 byte = tiles[tile_num * 32 + ty * 4 + (tx / 2)];
            u8 col_idx = (tx & 1) ? (byte >> 4) : (byte & 0x0F);

            if (col_idx != 0) {
                u16 bgr = bg_palette[pal_bank * 16 + col_idx];
                scanline[x] = BGR555_TO_RGBA32(bgr);
            }
        }
    }

    // Render OAM Sprites on this scanline
    u16* obj_palette = (u16*)(mmu->pram + 0x200);
    u16* oam = (u16*)mmu->oam;

    for (int i = 127; i >= 0; i--) {
        u16 attr0 = oam[i * 4 + 0];
        u16 attr1 = oam[i * 4 + 1];
        u16 attr2 = oam[i * 4 + 2];

        // Check if disabled / hidden
        if ((attr0 & 0x0300) == 0x0200) continue;

        int spr_y = attr0 & 0xFF;
        if (spr_y >= 160) spr_y -= 256;

        int shape = (attr0 >> 14) & 0x03;
        int size  = (attr1 >> 14) & 0x03;

        // Determine sprite width & height (standard square default)
        int width = 8, height = 8;
        if (shape == 0) { // Square
            if (size == 0) { width = 8; height = 8; }
            else if (size == 1) { width = 16; height = 16; }
            else if (size == 2) { width = 32; height = 32; }
            else if (size == 3) { width = 64; height = 64; }
        } else if (shape == 1) { // Wide
            if (size == 0) { width = 16; height = 8; }
            else if (size == 1) { width = 32; height = 8; }
            else if (size == 2) { width = 32; height = 16; }
            else if (size == 3) { width = 64; height = 32; }
        } else if (shape == 2) { // Tall
            if (size == 0) { width = 8; height = 16; }
            else if (size == 1) { width = 8; height = 32; }
            else if (size == 2) { width = 16; height = 32; }
            else if (size == 3) { width = 32; height = 64; }
        }

        if (line < spr_y || line >= spr_y + height) continue;

        int spr_x = attr1 & 0x1FF;
        if (spr_x >= 240) spr_x -= 512;

        bool flip_h = (attr1 & (1 << 12)) != 0;
        bool flip_v = (attr1 & (1 << 13)) != 0;
        u16 base_tile = attr2 & 0x3FF;
        u8 pal_bank   = (attr2 >> 12) & 0x0F;

        int fine_y = line - spr_y;
        if (flip_v) fine_y = (height - 1) - fine_y;

        int tile_row = fine_y >> 3;
        int sub_y = fine_y & 7;

        for (int sx = 0; sx < width; sx++) {
            int px = spr_x + sx;
            if (px < 0 || px >= GBA_SCREEN_WIDTH) continue;

            int cur_sx = flip_h ? ((width - 1) - sx) : sx;
            int tile_col = cur_sx >> 3;
            int sub_x = cur_sx & 7;

            u32 tile_num = base_tile + (tile_row * 32) + tile_col;
            u8* spr_tile = mmu->vram + 0x10000 + (tile_num * 32);

            u8 byte = spr_tile[sub_y * 4 + (sub_x / 2)];
            u8 col_idx = (sub_x & 1) ? (byte >> 4) : (byte & 0x0F);

            if (col_idx != 0) {
                u16 bgr = obj_palette[pal_bank * 16 + col_idx];
                scanline[px] = BGR555_TO_RGBA32(bgr);
            }
        }
    }
}

void video_render_frame(GbaVideo* video, GbaMMU* mmu) {
    for (u16 line = 0; line < GBA_SCREEN_HEIGHT; line++) {
        video_render_scanline(video, mmu, line);
    }
}

