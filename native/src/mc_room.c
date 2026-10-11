#include "mc_room.h"
#include "gba_lz77.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void room_init(Room* room, u16 width, u16 height) {
    memset(room, 0, sizeof(Room));
    room->width_metatiles = width;
    room->height_metatiles = height;

    u32 total_tiles = width * height;
    room->metatiles = (u16*)calloc(total_tiles, sizeof(u16));
    room->collision_flags = (u8*)calloc(total_tiles, sizeof(u8));

    // Default Minish Stump at center clearing (14, 12)
    room->stump_x = 14 * 16;
    room->stump_y = 12 * 16;
}

bool room_load_from_rom(Room* room, const u8* rom, u32 rom_size, u32 lz77_offset) {
    if (!room || !rom || lz77_offset >= rom_size) return false;

    u32 uncomp_size = lz77_get_uncompressed_size(&rom[lz77_offset]);
    if (uncomp_size == 0 || uncomp_size > 65536) {
        fprintf(stderr, "[Room] Invalid LZ77 stream at ROM 0x%06X\n", lz77_offset);
        return false;
    }

    u8* decomp_buffer = (u8*)malloc(uncomp_size);
    u32 decomp_len = lz77_decompress(&rom[lz77_offset], 12000, decomp_buffer, uncomp_size);

    u32 total_words = decomp_len / 2;
    u32 copy_words = (total_words < (u32)(room->width_metatiles * room->height_metatiles))
                   ? total_words
                   : (u32)(room->width_metatiles * room->height_metatiles);

    const u16* words = (const u16*)decomp_buffer;
    for (u32 i = 0; i < copy_words; i++) {
        room->metatiles[i] = words[i];
    }

    free(decomp_buffer);
    printf("[Room] Loaded %u metatiles from ROM @ 0x%06X\n", copy_words, lz77_offset);
    return true;
}

u8 room_get_collision(const Room* room, s16 pixel_x, s16 pixel_y) {
    if (!room || pixel_x < 0 || pixel_y < 0) return COLLISION_SOLID;

    u16 tx = (u16)(pixel_x / 16);
    u16 ty = (u16)(pixel_y / 16);

    if (tx >= room->width_metatiles || ty >= room->height_metatiles) {
        return COLLISION_SOLID;
    }

    return room->collision_flags[ty * room->width_metatiles + tx];
}

void room_update_camera(Room* room, s16 target_x, s16 target_y) {
    if (!room) return;

    s16 desired_cam_x = target_x - (GBA_SCREEN_WIDTH / 2);
    s16 desired_cam_y = target_y - (GBA_SCREEN_HEIGHT / 2);

    s16 max_cam_x = (s16)(room->width_metatiles * 16) - GBA_SCREEN_WIDTH;
    s16 max_cam_y = (s16)(room->height_metatiles * 16) - GBA_SCREEN_HEIGHT;

    if (max_cam_x < 0) max_cam_x = 0;
    if (max_cam_y < 0) max_cam_y = 0;

    if (desired_cam_x < 0) desired_cam_x = 0;
    if (desired_cam_x > max_cam_x) desired_cam_x = max_cam_x;

    if (desired_cam_y < 0) desired_cam_y = 0;
    if (desired_cam_y > max_cam_y) desired_cam_y = max_cam_y;

    room->camera_x = desired_cam_x;
    room->camera_y = desired_cam_y;
}

void room_render(const Room* room, const AssetManager* assets, u32* fb) {
    if (!room || !assets || !assets->loaded || !fb) return;

    int start_col = room->camera_x / 16;
    int end_col   = (room->camera_x + GBA_SCREEN_WIDTH) / 16 + 1;
    int start_row = room->camera_y / 16;
    int end_row   = (room->camera_y + GBA_SCREEN_HEIGHT) / 16 + 1;

    if (start_col < 0) start_col = 0;
    if (end_col > room->width_metatiles) end_col = room->width_metatiles;
    if (start_row < 0) start_row = 0;
    if (end_row > room->height_metatiles) end_row = room->height_metatiles;

    for (int r = start_row; r < end_row; r++) {
        for (int c = start_col; c < end_col; c++) {
            u16 tile_val = room->metatiles[r * room->width_metatiles + c];

            // Map overworld tile value to authentic tileset index
            u16 base_tile = tile_val % 512;
            if (base_tile == 0) base_tile = 10; // Default grass tile

            int screen_x = c * 16 - room->camera_x;
            int screen_y = r * 16 - room->camera_y;

            // Draw 4 8x8 quadrants to form the 16x16 metatile
            assets_draw_tile(assets, fb, screen_x,     screen_y,     base_tile);
            assets_draw_tile(assets, fb, screen_x + 8, screen_y,     base_tile + 1);
            assets_draw_tile(assets, fb, screen_x,     screen_y + 8, base_tile + 16);
            assets_draw_tile(assets, fb, screen_x + 8, screen_y + 8, base_tile + 17);
        }
    }

    // Draw Minish Stump Portal if visible
    int stump_sx = room->stump_x - room->camera_x;
    int stump_sy = room->stump_y - room->camera_y;
    if (stump_sx > -16 && stump_sx < GBA_SCREEN_WIDTH && stump_sy > -16 && stump_sy < GBA_SCREEN_HEIGHT) {
        assets_draw_tile(assets, fb, stump_sx,     stump_sy,     280);
        assets_draw_tile(assets, fb, stump_sx + 8, stump_sy,     281);
        assets_draw_tile(assets, fb, stump_sx,     stump_sy + 8, 296);
        assets_draw_tile(assets, fb, stump_sx + 8, stump_sy + 8, 297);
    }
}

void room_free(Room* room) {
    if (room->metatiles) {
        free(room->metatiles);
        room->metatiles = NULL;
    }
    if (room->collision_flags) {
        free(room->collision_flags);
        room->collision_flags = NULL;
    }
}
