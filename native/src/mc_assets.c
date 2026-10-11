#include "mc_assets.h"
#include "gba_lz77.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper to decode one 8x8 4bpp tile into a 32-bit RGBA buffer
static void decode_tile_raw(const u8* tile_bytes, const u32* palette, u32* out_pixels, u32 stride, bool opaque) {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x += 2) {
            u8 b = tile_bytes[y * 4 + (x / 2)];
            u8 c0 = b & 0x0F;
            u8 c1 = (b >> 4) & 0x0F;

            u32 col0 = (c0 == 0 && !opaque) ? 0 : palette[c0];
            u32 col1 = (c1 == 0 && !opaque) ? 0 : palette[c1];

            out_pixels[y * stride + x] = col0;
            out_pixels[y * stride + x + 1] = col1;
        }
    }
}

// Assemble a 16x24 Link frame from 6 8x8 tiles (t0..t5)
static void assemble_link_16x24(const u8* all_link_tiles, const u32* pal,
                               int t0, int t1, int t2, int t3, int t4, int t5,
                               u32* sheet, int col, int row, bool flip_h) {
    u32 tile_pixels[6][64]; // 6 tiles * 64 pixels each
    int tiles[6] = { t0, t1, t2, t3, t4, t5 };

    for (int i = 0; i < 6; i++) {
        decode_tile_raw(&all_link_tiles[tiles[i] * 32], pal, tile_pixels[i], 8, false);
    }

    int frame_x0 = col * 32 + 8; // Center 16px inside 32px frame
    int frame_y0 = row * 32 + 4; // Center 24px inside 32px frame

    for (int ty = 0; ty < 3; ty++) { // 3 rows of 8px = 24px
        for (int tx = 0; tx < 2; tx++) { // 2 cols of 8px = 16px
            int t_idx = ty * 2 + tx;
            const u32* t_pix = tile_pixels[t_idx];

            for (int py = 0; py < 8; py++) {
                for (int px = 0; px < 8; px++) {
                    int final_px = flip_h ? (15 - (tx * 8 + px)) : (tx * 8 + px);
                    int dest_x = frame_x0 + final_px;
                    int dest_y = frame_y0 + (ty * 8 + py);

                    if (dest_x >= 0 && dest_x < LINK_SHEET_W && dest_y >= 0 && dest_y < LINK_SHEET_H) {
                        u32 c = t_pix[py * 8 + px];
                        if ((c & 0xFF000000) != 0) {
                            sheet[dest_y * LINK_SHEET_W + dest_x] = c;
                        }
                    }
                }
            }
        }
    }
}

bool assets_init(AssetManager* assets, const u8* rom, u32 rom_size) {
    if (!assets || !rom || rom_size < 0x600000) return false;
    memset(assets, 0, sizeof(AssetManager));

    printf("[Assets] Loading authentic GBA assets from ROM...\n");

    // 1. Load Palettes from ROM @ 5910144 (0x5A2E80)
    u32 pal_base = 5910144;
    for (int p = 0; p < 16; p++) {
        for (int c = 0; c < 16; c++) {
            u32 offset = pal_base + (p * 32) + (c * 2);
            u16 bgr = (u16)rom[offset] | ((u16)rom[offset + 1] << 8);
            assets->bg_palettes[p][c] = BGR555_TO_RGBA32(bgr);
            assets->obj_palettes[p][c] = BGR555_TO_RGBA32(bgr);
        }
    }

    // 2. Load Minish Woods Environment Tileset @ 3296948 (0x324EB4)
    u32 lz77_env_offset = 3296948;
    u32 env_uncomp_size = lz77_get_uncompressed_size(&rom[lz77_env_offset]);
    if (env_uncomp_size > 0 && env_uncomp_size <= 65536) {
        u8* env_raw = (u8*)malloc(env_uncomp_size);
        lz77_decompress(&rom[lz77_env_offset], 12000, env_raw, env_uncomp_size);

        u32 total_env_tiles = env_uncomp_size / 32;
        for (u32 t = 0; t < total_env_tiles && t < 512; t++) {
            int pal_idx = (t < 120) ? 2 : (t < 250) ? 4 : (t < 350) ? 1 : 2;
            int tx = (t % 16) * 8;
            int ty = (t / 16) * 8;

            u32 tile_pixels[64];
            decode_tile_raw(&env_raw[t * 32], assets->bg_palettes[pal_idx], tile_pixels, 8, true);

            for (int y = 0; y < 8; y++) {
                for (int x = 0; x < 8; x++) {
                    assets->env_atlas[(ty + y) * ENV_ATLAS_W + (tx + x)] = tile_pixels[y * 8 + x];
                }
            }
        }
        free(env_raw);
        printf("[Assets] Decompressed %u environment tiles into atlas (%dx%d).\n",
               total_env_tiles, ENV_ATLAS_W, ENV_ATLAS_H);
    }

    // 3. Load Link's Sprites from ROM @ 0x13AE14 (gSprite_Link)
    u32 link_offset = 0x13AE14;
    const u8* link_tiles = &rom[link_offset];
    const u32* link_pal = assets->obj_palettes[14]; // Link's authentic palette 14

    // Assemble Link Frames
    // Row 0: Idle (South, East, North, West)
    assemble_link_16x24(link_tiles, link_pal, 0, 1, 2, 3, 4, 5, assets->link_sheet, 0, 0, false);
    assemble_link_16x24(link_tiles, link_pal, 7, 8, 9, 10, 11, 12, assets->link_sheet, 1, 0, false);
    assemble_link_16x24(link_tiles, link_pal, 14, 15, 16, 17, 18, 19, assets->link_sheet, 2, 0, false);
    assemble_link_16x24(link_tiles, link_pal, 7, 8, 9, 10, 11, 12, assets->link_sheet, 3, 0, true); // West = flip East

    // Row 1: Walk South (4 frames)
    assemble_link_16x24(link_tiles, link_pal, 0, 1, 2, 3, 4, 5, assets->link_sheet, 0, 1, false);
    assemble_link_16x24(link_tiles, link_pal, 21, 22, 23, 24, 25, 26, assets->link_sheet, 1, 1, false);
    assemble_link_16x24(link_tiles, link_pal, 28, 29, 30, 31, 32, 33, assets->link_sheet, 2, 1, false);
    assemble_link_16x24(link_tiles, link_pal, 35, 36, 37, 38, 39, 40, assets->link_sheet, 3, 1, false);

    // Row 2: Walk East (4 frames)
    assemble_link_16x24(link_tiles, link_pal, 7, 8, 9, 10, 11, 12, assets->link_sheet, 0, 2, false);
    assemble_link_16x24(link_tiles, link_pal, 63, 64, 65, 66, 67, 68, assets->link_sheet, 1, 2, false);
    assemble_link_16x24(link_tiles, link_pal, 70, 71, 72, 73, 74, 75, assets->link_sheet, 2, 2, false);
    assemble_link_16x24(link_tiles, link_pal, 77, 78, 79, 80, 81, 82, assets->link_sheet, 3, 2, false);

    // Row 3: Walk North (4 frames)
    assemble_link_16x24(link_tiles, link_pal, 14, 15, 16, 17, 18, 19, assets->link_sheet, 0, 3, false);
    assemble_link_16x24(link_tiles, link_pal, 42, 43, 44, 45, 46, 47, assets->link_sheet, 1, 3, false);
    assemble_link_16x24(link_tiles, link_pal, 49, 50, 51, 52, 53, 54, assets->link_sheet, 2, 3, false);
    assemble_link_16x24(link_tiles, link_pal, 56, 57, 58, 59, 60, 61, assets->link_sheet, 3, 3, false);

    // Row 4: Walk West (4 frames - flip East)
    assemble_link_16x24(link_tiles, link_pal, 7, 8, 9, 10, 11, 12, assets->link_sheet, 0, 4, true);
    assemble_link_16x24(link_tiles, link_pal, 63, 64, 65, 66, 67, 68, assets->link_sheet, 1, 4, true);
    assemble_link_16x24(link_tiles, link_pal, 70, 71, 72, 73, 74, 75, assets->link_sheet, 2, 4, true);
    assemble_link_16x24(link_tiles, link_pal, 77, 78, 79, 80, 81, 82, assets->link_sheet, 3, 4, true);

    // Row 5: Slash Attacks (S, E, N, W)
    assemble_link_16x24(link_tiles, link_pal, 84, 85, 86, 87, 88, 89, assets->link_sheet, 0, 5, false);
    assemble_link_16x24(link_tiles, link_pal, 98, 99, 100, 101, 102, 103, assets->link_sheet, 1, 5, false);
    assemble_link_16x24(link_tiles, link_pal, 112, 113, 114, 115, 116, 117, assets->link_sheet, 2, 5, false);
    assemble_link_16x24(link_tiles, link_pal, 98, 99, 100, 101, 102, 103, assets->link_sheet, 3, 5, true);

    // Row 6: Roll Animation (4 frames)
    assemble_link_16x24(link_tiles, link_pal, 126, 127, 128, 129, 130, 131, assets->link_sheet, 0, 6, false);
    assemble_link_16x24(link_tiles, link_pal, 133, 134, 135, 136, 137, 138, assets->link_sheet, 1, 6, false);
    assemble_link_16x24(link_tiles, link_pal, 140, 141, 142, 143, 144, 145, assets->link_sheet, 2, 6, false);
    assemble_link_16x24(link_tiles, link_pal, 147, 148, 149, 150, 151, 152, assets->link_sheet, 3, 6, false);

    // Row 7: Minish Link (Tiny 8x12 form scaled from center pixels)
    for (int dir = 0; dir < 4; dir++) {
        int src_col = dir;
        int dst_col = dir;
        for (int y = 0; y < 12; y++) {
            for (int x = 0; x < 8; x++) {
                u32 src_pix = assets->link_sheet[(src_col * 32 + 8 + x * 2) + ((0 * 32 + 4 + y * 2) * LINK_SHEET_W)];
                if ((src_pix & 0xFF000000) != 0) {
                    assets->link_sheet[(7 * 32 + 10 + y) * LINK_SHEET_W + (dst_col * 32 + 12 + x)] = src_pix;
                }
            }
        }
    }

    printf("[Assets] Assembled 64 Link animation frames into sprite sheet (%dx%d).\n",
           LINK_SHEET_W, LINK_SHEET_H);

    assets->loaded = true;
    return true;
}

void assets_draw_tile(const AssetManager* assets, u32* fb, int dst_x, int dst_y, u16 tile_idx) {
    if (!assets || !assets->loaded || !fb) return;
    tile_idx %= 512;

    int src_x = (tile_idx % 16) * 8;
    int src_y = (tile_idx / 16) * 8;

    for (int y = 0; y < 8; y++) {
        int dy = dst_y + y;
        if (dy < 0 || dy >= GBA_SCREEN_HEIGHT) continue;

        for (int x = 0; x < 8; x++) {
            int dx = dst_x + x;
            if (dx < 0 || dx >= GBA_SCREEN_WIDTH) continue;

            u32 c = assets->env_atlas[(src_y + y) * ENV_ATLAS_W + (src_x + x)];
            fb[dy * GBA_SCREEN_WIDTH + dx] = c;
        }
    }
}

void assets_draw_link_frame(const AssetManager* assets, u32* fb, int dst_x, int dst_y, int col, int row) {
    if (!assets || !assets->loaded || !fb) return;
    col &= 7;
    row &= 7;

    int src_x0 = col * 32;
    int src_y0 = row * 32;

    for (int y = 0; y < 32; y++) {
        int dy = dst_y + y;
        if (dy < 0 || dy >= GBA_SCREEN_HEIGHT) continue;

        for (int x = 0; x < 32; x++) {
            int dx = dst_x + x;
            if (dx < 0 || dx >= GBA_SCREEN_WIDTH) continue;

            u32 color = assets->link_sheet[(src_y0 + y) * LINK_SHEET_W + (src_x0 + x)];
            if ((color & 0xFF000000) != 0) { // Non-transparent pixel
                fb[dy * GBA_SCREEN_WIDTH + dx] = color;
            }
        }
    }
}

