#ifndef GBA_LZ77_H
#define GBA_LZ77_H

#include "gba_types.h"

// Decompresses GBA BIOS LZ77 (type 0x10) data from source buffer into destination.
// Returns the number of uncompressed bytes written, or 0 on error.
u32 lz77_decompress(const u8* src, u32 src_len, u8* dst, u32 dst_max_len);

// Helper to inspect the uncompressed size from the 4-byte GBA header (0x10, size_lo, size_mid, size_hi)
u32 lz77_get_uncompressed_size(const u8* src);

#endif // GBA_LZ77_H

