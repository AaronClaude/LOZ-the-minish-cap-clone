#include "gba_mmu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool mmu_init(GbaMMU* mmu) {
    memset(mmu, 0, sizeof(GbaMMU));

    mmu->ewram = (u8*)calloc(1, EWRAM_SIZE);
    mmu->iwram = (u8*)calloc(1, IWRAM_SIZE);
    mmu->vram  = (u8*)calloc(1, VRAM_SIZE);
    mmu->pram  = (u8*)calloc(1, PRAM_SIZE);
    mmu->oam   = (u8*)calloc(1, OAM_SIZE);
    mmu->sram  = (u8*)calloc(1, SRAM_SIZE);

    if (!mmu->ewram || !mmu->iwram || !mmu->vram || !mmu->pram || !mmu->oam || !mmu->sram) {
        mmu_free(mmu);
        return false;
    }

    strcpy(mmu->save_path, "minish_cap.sav");
    return true;
}

bool mmu_load_rom(GbaMMU* mmu, const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "[MMU] Failed to open ROM file: %s\n", filepath);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > ROM_MAX_SIZE) {
        fprintf(stderr, "[MMU] Invalid ROM size: %ld bytes\n", size);
        fclose(f);
        return false;
    }

    mmu->rom = (u8*)calloc(1, ROM_MAX_SIZE);
    if (!mmu->rom) {
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(mmu->rom, 1, size, f);
    fclose(f);

    mmu->rom_size = (u32)read_bytes;
    
    // Calculate power of 2 mask for ROM mirroring
    u32 mask = 1;
    while (mask < mmu->rom_size) mask <<= 1;
    mmu->rom_mask = mask - 1;

    // Parse GBA ROM Header (0xA0 - 0xBC)
    memcpy(mmu->title, &mmu->rom[0xA0], 12);
    mmu->title[12] = '\0';

    memcpy(mmu->game_code, &mmu->rom[0xAC], 4);
    mmu->game_code[4] = '\0';

    memcpy(mmu->maker_code, &mmu->rom[0xB0], 2);
    mmu->maker_code[2] = '\0';

    mmu->version = mmu->rom[0xBC];

    printf("[MMU] Loaded ROM: \"%s\" | Code: [%s] | Size: %u bytes (%.1f MB)\n",
           mmu->title, mmu->game_code, mmu->rom_size, (float)mmu->rom_size / (1024.0f * 1024.0f));

    mmu_load_sram(mmu);
    return true;
}

void mmu_free(GbaMMU* mmu) {
    if (mmu->sram_dirty) {
        mmu_save_sram(mmu);
    }
    if (mmu->ewram) { free(mmu->ewram); mmu->ewram = NULL; }
    if (mmu->iwram) { free(mmu->iwram); mmu->iwram = NULL; }
    if (mmu->vram)  { free(mmu->vram);  mmu->vram  = NULL; }
    if (mmu->pram)  { free(mmu->pram);  mmu->pram  = NULL; }
    if (mmu->oam)   { free(mmu->oam);   mmu->oam   = NULL; }
    if (mmu->rom)   { free(mmu->rom);   mmu->rom   = NULL; }
    if (mmu->sram)  { free(mmu->sram);  mmu->sram  = NULL; }
}

u8 mmu_read8(GbaMMU* mmu, u32 addr) {
    u32 region = addr >> 24;
    u32 offset = addr & 0x00FFFFFF;

    switch (region) {
        case 0x02: return mmu->ewram[offset % EWRAM_SIZE];
        case 0x03: return mmu->iwram[offset % IWRAM_SIZE];
        case 0x05: return mmu->pram[offset % PRAM_SIZE];
        case 0x06: return mmu->vram[offset % VRAM_SIZE];
        case 0x07: return mmu->oam[offset % OAM_SIZE];
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
            if (mmu->rom) return mmu->rom[offset & mmu->rom_mask];
            return 0xFF;
        case 0x0E:
            return mmu->sram[offset % SRAM_SIZE];
        default:
            return 0;
    }
}

u16 mmu_read16(GbaMMU* mmu, u32 addr) {
    addr &= ~1; // 16-bit alignment
    return (u16)mmu_read8(mmu, addr) | ((u16)mmu_read8(mmu, addr + 1) << 8);
}

u32 mmu_read32(GbaMMU* mmu, u32 addr) {
    addr &= ~3; // 32-bit alignment
    return (u32)mmu_read16(mmu, addr) | ((u32)mmu_read16(mmu, addr + 2) << 16);
}

void mmu_write8(GbaMMU* mmu, u32 addr, u8 val) {
    u32 region = addr >> 24;
    u32 offset = addr & 0x00FFFFFF;

    switch (region) {
        case 0x02:
            mmu->ewram[offset % EWRAM_SIZE] = val;
            break;
        case 0x03:
            mmu->iwram[offset % IWRAM_SIZE] = val;
            break;
        case 0x05:
            mmu->pram[offset % PRAM_SIZE] = val;
            break;
        case 0x06:
            mmu->vram[offset % VRAM_SIZE] = val;
            break;
        case 0x07:
            mmu->oam[offset % OAM_SIZE] = val;
            break;
        case 0x0E:
            mmu->sram[offset % SRAM_SIZE] = val;
            mmu->sram_dirty = true;
            break;
        default:
            break;
    }
}

void mmu_write16(GbaMMU* mmu, u32 addr, u16 val) {
    addr &= ~1;
    mmu_write8(mmu, addr, (u8)(val & 0xFF));
    mmu_write8(mmu, addr + 1, (u8)(val >> 8));
}

void mmu_write32(GbaMMU* mmu, u32 addr, u32 val) {
    addr &= ~3;
    mmu_write16(mmu, addr, (u16)(val & 0xFFFF));
    mmu_write16(mmu, addr + 2, (u16)(val >> 16));
}

void mmu_save_sram(GbaMMU* mmu) {
    if (!mmu->sram) return;
    FILE* f = fopen(mmu->save_path, "wb");
    if (f) {
        fwrite(mmu->sram, 1, SRAM_SIZE, f);
        fclose(f);
        mmu->sram_dirty = false;
        printf("[MMU] Battery save persisted to %s\n", mmu->save_path);
    }
}

void mmu_load_sram(GbaMMU* mmu) {
    if (!mmu->sram) return;
    FILE* f = fopen(mmu->save_path, "rb");
    if (f) {
        fread(mmu->sram, 1, SRAM_SIZE, f);
        fclose(f);
        printf("[MMU] Loaded battery save from %s\n", mmu->save_path);
    }
}

