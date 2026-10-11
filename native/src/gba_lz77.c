#include "gba_lz77.h"
#include <stdio.h>

u32 lz77_get_uncompressed_size(const u8* src) {
    if (!src || src[0] != 0x10) return 0;
    return (u32)src[1] | ((u32)src[2] << 8) | ((u32)src[3] << 16);
}

u32 lz77_decompress(const u8* src, u32 src_len, u8* dst, u32 dst_max_len) {
    if (!src || !dst || src_len < 4) return 0;
    if (src[0] != 0x10) {
        fprintf(stderr, "[LZ77] Invalid header magic: 0x%02X (expected 0x10)\n", src[0]);
        return 0;
    }

    u32 uncompressed_size = lz77_get_uncompressed_size(src);
    if (uncompressed_size > dst_max_len) {
        fprintf(stderr, "[LZ77] Destination buffer too small (%u > %u)\n", uncompressed_size, dst_max_len);
        return 0;
    }

    u32 src_pos = 4;
    u32 dst_pos = 0;

    while (src_pos < src_len && dst_pos < uncompressed_size) {
        u8 flag_byte = src[src_pos++];

        for (int i = 0; i < 8 && dst_pos < uncompressed_size; i++) {
            if ((flag_byte & (0x80 >> i)) == 0) {
                // Literal uncompressed byte
                if (src_pos >= src_len) break;
                dst[dst_pos++] = src[src_pos++];
            } else {
                // Compressed block (length and offset)
                if (src_pos + 1 >= src_len) break;
                u8 b1 = src[src_pos++];
                u8 b2 = src[src_pos++];

                u32 length = (b1 >> 4) + 3;
                u32 disp   = (((b1 & 0x0F) << 8) | b2) + 1;

                if (disp > dst_pos) {
                    fprintf(stderr, "[LZ77] Displacement exceeds current output (disp: %u, pos: %u)\n", disp, dst_pos);
                    return 0;
                }

                u32 copy_src = dst_pos - disp;
                for (u32 j = 0; j < length && dst_pos < uncompressed_size; j++) {
                    dst[dst_pos++] = dst[copy_src++];
                }
            }
        }
    }

    return dst_pos;
}

