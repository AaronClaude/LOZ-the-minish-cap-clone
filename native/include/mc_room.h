#ifndef MC_ROOM_H
#define MC_ROOM_H

#include "gba_types.h"
#include "gba_mmu.h"
#include "mc_assets.h"

// Collision flag types in Minish Cap
#define COLLISION_NONE          0x00
#define COLLISION_SOLID         0x01
#define COLLISION_WATER         0x02
#define COLLISION_LEDGE_DOWN    0x03
#define COLLISION_MINISH_HOLE   0x04
#define COLLISION_PORTAL_STUMP  0x05
#define COLLISION_BUSH          0x06
#define COLLISION_POT           0x07

typedef struct {
    u16 width_metatiles;
    u16 height_metatiles;
    u16* metatiles;          // Layer 1 metatiles (16x16 pixels each)
    u8* collision_flags;     // Collision matrix
    s16 camera_x;
    s16 camera_y;
    u16 room_id;
    u16 area_id;

    // Interactive Minish Stump coordinates
    s16 stump_x;
    s16 stump_y;
} Room;

void room_init(Room* room, u16 width, u16 height);
bool room_load_from_rom(Room* room, const u8* rom, u32 rom_size, u32 lz77_offset);
u8   room_get_collision(const Room* room, s16 pixel_x, s16 pixel_y);
void room_update_camera(Room* room, s16 target_x, s16 target_y);
void room_render(const Room* room, const AssetManager* assets, u32* fb);
void room_free(Room* room);

#endif // MC_ROOM_H
