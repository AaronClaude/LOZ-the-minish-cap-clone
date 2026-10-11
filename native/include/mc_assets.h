#ifndef MC_ASSETS_H
#define MC_ASSETS_H

#include "gba_types.h"

#define ENV_ATLAS_W   128
#define ENV_ATLAS_H   256

#define LINK_SHEET_W  256
#define LINK_SHEET_H  256

#define ITEMS_ATLAS_W 128
#define ITEMS_ATLAS_H 128

typedef struct {
    u32 env_atlas[ENV_ATLAS_W * ENV_ATLAS_H];       // 512 8x8 tiles (16x32 grid)
    u32 link_sheet[LINK_SHEET_W * LINK_SHEET_H];    // 64 32x32 frames (8x8 grid)
    u32 items_atlas[ITEMS_ATLAS_W * ITEMS_ATLAS_H]; // 64 16x16 icons (8x8 grid)

    u32 bg_palettes[16][16];
    u32 obj_palettes[16][16];
    bool loaded;
} AssetManager;

bool assets_init(AssetManager* assets, const u8* rom, u32 rom_size);
void assets_draw_tile(const AssetManager* assets, u32* fb, int dst_x, int dst_y, u16 tile_idx);
void assets_draw_link_frame(const AssetManager* assets, u32* fb, int dst_x, int dst_y, int col, int row);
void assets_draw_icon(const AssetManager* assets, u32* fb, int dst_x, int dst_y, int col, int row);

#endif // MC_ASSETS_H

