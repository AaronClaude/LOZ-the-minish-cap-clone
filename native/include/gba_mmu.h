#ifndef GBA_MMU_H
#define GBA_MMU_H

#include "gba_types.h"

typedef struct {
    u8* ewram;       // 256 KB
    u8* iwram;       // 32 KB
    u8* vram;        // 96 KB
    u8* pram;        // 1 KB
    u8* oam;         // 1 KB
    u8* rom;         // Up to 16 MB
    u8* sram;        // 128 KB
    u32 rom_size;
    u32 rom_mask;

    // ROM Header information
    char title[13];
    char game_code[5];
    char maker_code[3];
    u8 version;

    // SRAM dirty flag
    bool sram_dirty;
    char save_path[512];
} GbaMMU;

bool mmu_init(GbaMMU* mmu);
bool mmu_load_rom(GbaMMU* mmu, const char* filepath);
void mmu_free(GbaMMU* mmu);

u8   mmu_read8(GbaMMU* mmu, u32 addr);
u16  mmu_read16(GbaMMU* mmu, u32 addr);
u32  mmu_read32(GbaMMU* mmu, u32 addr);

void mmu_write8(GbaMMU* mmu, u32 addr, u8 val);
void mmu_write16(GbaMMU* mmu, u32 addr, u16 val);
void mmu_write32(GbaMMU* mmu, u32 addr, u32 val);

void mmu_save_sram(GbaMMU* mmu);
void mmu_load_sram(GbaMMU* mmu);

#endif // GBA_MMU_H

