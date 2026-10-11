#ifndef GBA_TYPES_H
#define GBA_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Standard GBA decompilation integer types
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;

typedef u8        bool8;
typedef u32       bool32;

// GBA Native Screen Dimensions
#define GBA_SCREEN_WIDTH   240
#define GBA_SCREEN_HEIGHT  160
#define GBA_PIXEL_COUNT    (GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT)

// GBA Memory Architecture Offsets
#define REGION_BIOS        0x00000000
#define REGION_EWRAM       0x02000000
#define REGION_IWRAM       0x03000000
#define REGION_IO          0x04000000
#define REGION_PALETTE     0x05000000
#define REGION_VRAM        0x06000000
#define REGION_OAM         0x07000000
#define REGION_ROM         0x08000000
#define REGION_SRAM        0x0E000000

#define EWRAM_SIZE         0x00040000 // 256 KB
#define IWRAM_SIZE         0x00008000 // 32 KB
#define VRAM_SIZE          0x00018000 // 96 KB
#define PRAM_SIZE          0x00000400 // 1 KB
#define OAM_SIZE           0x00000400 // 1 KB
#define ROM_MAX_SIZE       0x01000000 // 16 MB
#define SRAM_SIZE          0x00020000 // 128 KB Flash / SRAM

// BGR555 to 32-bit RGBA8888 Color Converter
static inline u32 BGR555_TO_RGBA32(u16 color) {
    u8 r = (color & 0x1F) << 3;
    u8 g = ((color >> 5) & 0x1F) << 3;
    u8 b = ((color >> 10) & 0x1F) << 3;
    // Fill bottom 3 bits for linear color distribution
    r |= (r >> 5);
    g |= (g >> 5);
    b |= (b >> 5);
    return 0xFF000000 | (b << 16) | (g << 8) | r;
}

#endif // GBA_TYPES_H

