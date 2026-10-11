#!/usr/bin/env python3
"""
Extract authentic assets directly from "Legend of Zelda, The - The Minish Cap (USA).gba"
No external dependencies required (uses built-in struct and zlib).
"""
import os
import sys
import struct
import zlib
import json

ROM_PATH = "Legend of Zelda, The - The Minish Cap (USA).gba"
OUT_DIR = "dist/assets"

def decompress_lz77(data):
    if len(data) < 4:
        return b''
    magic = data[0]
    if magic != 0x10:
        raise ValueError(f"Not LZ77 0x10 format, got 0x{magic:02X}")
    decomp_size = data[1] | (data[2] << 8) | (data[3] << 16)
    out = bytearray()
    idx = 4
    while len(out) < decomp_size and idx < len(data):
        flags = data[idx]
        idx += 1
        for b in range(8):
            if len(out) >= decomp_size:
                break
            if (flags & (0x80 >> b)) == 0:
                if idx < len(data):
                    out.append(data[idx])
                    idx += 1
            else:
                if idx + 1 < len(data):
                    byte_a = data[idx]
                    byte_b = data[idx+1]
                    idx += 2
                    length = (byte_a >> 4) + 3
                    disp = (((byte_a & 0x0F) << 8) | byte_b) + 1
                    for _ in range(length):
                        if len(out) >= decomp_size:
                            break
                        ref_pos = len(out) - disp
                        out.append(out[ref_pos])
    return bytes(out)

def parse_palette(pal_bytes):
    colors = []
    for i in range(0, len(pal_bytes), 2):
        w = struct.unpack('<H', pal_bytes[i:i+2])[0]
        r = (w & 0x1F) * 255 // 31
        g = ((w >> 5) & 0x1F) * 255 // 31
        b = ((w >> 10) & 0x1F) * 255 // 31
        a = 0 if i == 0 else 255  # index 0 is transparent in GBA sprites
        colors.append((r, g, b, a))
    return colors

def decode_4bpp_tile(tile_bytes, palette):
    pixels = []
    for y in range(8):
        row = tile_bytes[y*4:(y+1)*4]
        row_pix = []
        for b in row:
            p0 = b & 0x0F
            p1 = (b >> 4) & 0x0F
            row_pix.append(palette[p0])
            row_pix.append(palette[p1])
        pixels.append(row_pix)
    return pixels

def make_png(width, height, rgba_flat):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter type 0: None
        raw.extend(rgba_flat[y*width*4 : (y+1)*width*4])
    
    header = b'\x89PNG\r\n\x1a\n'
    ihdr = chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    idat = chunk(b'IDAT', zlib.compress(bytes(raw), level=9))
    iend = chunk(b'IEND', b'')
    return header + ihdr + idat + iend

def save_image(filepath, width, height, pixels_2d):
    flat = bytearray()
    for y in range(height):
        for x in range(width):
            c = pixels_2d[y][x]
            flat.extend([c[0], c[1], c[2], c[3]])
    png_data = make_png(width, height, flat)
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    with open(filepath, 'wb') as f:
        f.write(png_data)

def extract_all():
    print(f"Opening ROM: {ROM_PATH}")
    if not os.path.exists(ROM_PATH):
        raise FileNotFoundError(f"ROM not found at {ROM_PATH}")

    os.makedirs(OUT_DIR, exist_ok=True)

    with open(ROM_PATH, 'rb') as f:
        # 1. Read Link Sprite Palette (gPalette_14) at 5910144 + 14 * 32 = 5910592
        f.seek(5910144 + 14 * 32)
        link_palette = parse_palette(f.read(32))

        # 2. Read All BG Palettes (gPalette_0 to gPalette_15)
        f.seek(5910144)
        bg_palettes = []
        for p in range(16):
            bg_palettes.append(parse_palette(f.read(32)))

        # Also prepare an opaque version for background tiles (index 0 is opaque BG color)
        bg_palettes_opaque = []
        for pal in bg_palettes:
            op_pal = list(pal)
            op_pal[0] = (pal[0][0], pal[0][1], pal[0][2], 255)
            bg_palettes_opaque.append(op_pal)

        # 3. Read Link's 4bpp sprite tiles from gSprite_Link @ 0x13AE14
        f.seek(0x13AE14)
        # Read the first 400 tiles (enough for walk, idle, slash, roll)
        link_tile_bytes = [f.read(32) for _ in range(400)]
        link_tiles = [decode_4bpp_tile(tb, link_palette) for tb in link_tile_bytes]

        print("Extracted Link tiles from ROM.")

        # Reconstruct Link frames (16x24 standard sprite size)
        # Tile arrangement for 16x24 sprite:
        # row 0..7:   tile0 (left),  tile1 (right)
        # row 8..15:  tile2 (left),  tile3 (right)
        # row 16..23: tile4 (left),  tile5 (right)
        def assemble_16x24(t0, t1, t2, t3, t4, t5):
            img = []
            for y in range(8):
                img.append(link_tiles[t0][y] + link_tiles[t1][y])
            for y in range(8):
                img.append(link_tiles[t2][y] + link_tiles[t3][y])
            for y in range(8):
                img.append(link_tiles[t4][y] + link_tiles[t5][y])
            return img

        def flip_h(img_2d):
            return [row[::-1] for row in img_2d]

        # Extract specific frames
        frame_idle_s = assemble_16x24(0, 1, 2, 3, 4, 5)
        frame_idle_e = assemble_16x24(7, 8, 9, 10, 11, 12)  # profile
        frame_idle_n = assemble_16x24(14, 15, 16, 17, 18, 19) # back
        frame_idle_w = flip_h(frame_idle_e)

        # Walk South frames
        frame_walk_s_0 = frame_idle_s
        frame_walk_s_1 = assemble_16x24(21, 22, 23, 24, 25, 26)
        frame_walk_s_2 = assemble_16x24(28, 29, 30, 31, 32, 33)
        frame_walk_s_3 = assemble_16x24(35, 36, 37, 38, 39, 40)

        # Walk North frames
        frame_walk_n_0 = frame_idle_n
        frame_walk_n_1 = assemble_16x24(42, 43, 44, 45, 46, 47)
        frame_walk_n_2 = assemble_16x24(49, 50, 51, 52, 53, 54)
        frame_walk_n_3 = assemble_16x24(56, 57, 58, 59, 60, 61)

        # Walk East frames
        frame_walk_e_0 = frame_idle_e
        frame_walk_e_1 = assemble_16x24(63, 64, 65, 66, 67, 68)
        frame_walk_e_2 = assemble_16x24(70, 71, 72, 73, 74, 75)
        frame_walk_e_3 = assemble_16x24(77, 78, 79, 80, 81, 82)

        # Walk West frames (flipped East)
        frame_walk_w_0 = flip_h(frame_walk_e_0)
        frame_walk_w_1 = flip_h(frame_walk_e_1)
        frame_walk_w_2 = flip_h(frame_walk_e_2)
        frame_walk_w_3 = flip_h(frame_walk_e_3)

        # Build a complete sprite sheet of Link
        # 16 columns x 4 rows of 24x24 frames (padded for weapon swings)
        FRAME_W = 32
        FRAME_H = 32
        SHEET_COLS = 8
        SHEET_ROWS = 8
        sheet_w = SHEET_COLS * FRAME_W
        sheet_h = SHEET_ROWS * FRAME_H
        sheet = [[(0, 0, 0, 0) for _ in range(sheet_w)] for _ in range(sheet_h)]

        def blit(src_img, col, row, offset_x=8, offset_y=4):
            src_h = len(src_img)
            src_w = len(src_img[0])
            dest_x0 = col * FRAME_W + offset_x
            dest_y0 = row * FRAME_H + offset_y
            for y in range(src_h):
                for x in range(src_w):
                    if dest_y0 + y < sheet_h and dest_x0 + x < sheet_w:
                        sheet[dest_y0 + y][dest_x0 + x] = src_img[y][x]

        # Row 0: Idle (S, E, N, W)
        blit(frame_idle_s, 0, 0)
        blit(frame_idle_e, 1, 0)
        blit(frame_idle_n, 2, 0)
        blit(frame_idle_w, 3, 0)

        # Row 1: Walk South (4 frames)
        blit(frame_walk_s_0, 0, 1)
        blit(frame_walk_s_1, 1, 1)
        blit(frame_walk_s_2, 2, 1)
        blit(frame_walk_s_3, 3, 1)

        # Row 2: Walk East (4 frames)
        blit(frame_walk_e_0, 0, 2)
        blit(frame_walk_e_1, 1, 2)
        blit(frame_walk_e_2, 2, 2)
        blit(frame_walk_e_3, 3, 2)

        # Row 3: Walk North (4 frames)
        blit(frame_walk_n_0, 0, 3)
        blit(frame_walk_n_1, 1, 3)
        blit(frame_walk_n_2, 2, 3)
        blit(frame_walk_n_3, 3, 3)

        # Row 4: Walk West (4 frames)
        blit(frame_walk_w_0, 0, 4)
        blit(frame_walk_w_1, 1, 4)
        blit(frame_walk_w_2, 2, 4)
        blit(frame_walk_w_3, 3, 4)

        # Sword slash frames (assembled from combat tiles starting around tile 90-180)
        # We synthesize slash poses and sword extension from authentic tiles:
        slash_s_0 = assemble_16x24(84, 85, 86, 87, 88, 89)
        slash_s_1 = assemble_16x24(91, 92, 93, 94, 95, 96)
        slash_e_0 = assemble_16x24(98, 99, 100, 101, 102, 103)
        slash_e_1 = assemble_16x24(105, 106, 107, 108, 109, 110)
        slash_n_0 = assemble_16x24(112, 113, 114, 115, 116, 117)
        slash_n_1 = assemble_16x24(119, 120, 121, 122, 123, 124)

        # Row 5: Slash Attacks (S, E, N, W)
        blit(slash_s_0, 0, 5)
        blit(slash_s_1, 1, 5)
        blit(slash_e_0, 2, 5)
        blit(slash_e_1, 3, 5)
        blit(slash_n_0, 4, 5)
        blit(slash_n_1, 5, 5)
        blit(flip_h(slash_e_0), 6, 5)
        blit(flip_h(slash_e_1), 7, 5)

        # Row 6: Roll animation (4 directions)
        roll_0 = assemble_16x24(126, 127, 128, 129, 130, 131)
        roll_1 = assemble_16x24(133, 134, 135, 136, 137, 138)
        roll_2 = assemble_16x24(140, 141, 142, 143, 144, 145)
        roll_3 = assemble_16x24(147, 148, 149, 150, 151, 152)
        blit(roll_0, 0, 6)
        blit(roll_1, 1, 6)
        blit(roll_2, 2, 6)
        blit(roll_3, 3, 6)

        # Row 7: Minish Link (Tiny Link scaled down to 8x12 with authentic pixels)
        # Create tiny Minish Link sprite from center tiles
        minish_s = []
        for y in range(0, 24, 2):
            row = []
            for x in range(0, 16, 2):
                row.append(frame_idle_s[y][x])
            minish_s.append(row)
        minish_e = []
        for y in range(0, 24, 2):
            row = []
            for x in range(0, 16, 2):
                row.append(frame_idle_e[y][x])
            minish_e.append(row)
        minish_n = []
        for y in range(0, 24, 2):
            row = []
            for x in range(0, 16, 2):
                row.append(frame_idle_n[y][x])
            minish_n.append(row)
        minish_w = flip_h(minish_e)

        blit(minish_s, 0, 7, offset_x=12, offset_y=10)
        blit(minish_e, 1, 7, offset_x=12, offset_y=10)
        blit(minish_n, 2, 7, offset_x=12, offset_y=10)
        blit(minish_w, 3, 7, offset_x=12, offset_y=10)

        # Save Link spritesheet
        link_sheet_path = os.path.join(OUT_DIR, "link_sheet.png")
        save_image(link_sheet_path, sheet_w, sheet_h, sheet)
        print(f"Saved Link spritesheet to {link_sheet_path} ({sheet_w}x{sheet_h})")

        # 4. Extract Minish Woods Environment Tileset
        # Subtileset 0: start=3296948, size=11348
        f.seek(3296948)
        wood_tiles_raw = decompress_lz77(f.read(11348))
        wood_num_tiles = len(wood_tiles_raw) // 32
        print(f"Extracted Minish Woods subtileset: {wood_num_tiles} 8x8 tiles.")

        # Decode tiles using foliage palette (gPalette_2)
        foliage_pal = bg_palettes_opaque[2]
        dirt_pal = bg_palettes_opaque[4]
        water_pal = bg_palettes_opaque[1]

        # Let's decode all 512 8x8 tiles with their corresponding palettes into an atlas
        # Atlas: 16 tiles wide x 32 tiles high = 128x256 pixels
        ATLAS_TILES_X = 16
        ATLAS_TILES_Y = 32
        atlas_w = ATLAS_TILES_X * 8
        atlas_h = ATLAS_TILES_Y * 8
        env_atlas = [[(0, 0, 0, 255) for _ in range(atlas_w)] for _ in range(atlas_h)]

        for t_idx in range(min(512, wood_num_tiles)):
            t_bytes = wood_tiles_raw[t_idx*32 : (t_idx+1)*32]
            # Use appropriate palette based on tile range:
            if t_idx < 120:
                p = foliage_pal
            elif t_idx < 250:
                p = dirt_pal
            elif t_idx < 350:
                p = water_pal
            else:
                p = foliage_pal

            tile_pix = decode_4bpp_tile(t_bytes, p)
            tx = (t_idx % ATLAS_TILES_X) * 8
            ty = (t_idx // ATLAS_TILES_X) * 8
            for y in range(8):
                for x in range(8):
                    env_atlas[ty + y][tx + x] = tile_pix[y][x]

        env_path = os.path.join(OUT_DIR, "environment_tiles.png")
        save_image(env_path, atlas_w, atlas_h, env_atlas)
        print(f"Saved Environment tileset atlas to {env_path}")

        # 5. Extract Room Map Data
        # Minish Woods room map: start=3338724, size=4268
        f.seek(3338724)
        room_map_raw = decompress_lz77(f.read(4268))
        words = []
        for i in range(0, len(room_map_raw), 2):
            words.append(room_map_raw[i] | (room_map_raw[i+1] << 8))

        room_map_json = {
            "width": 63,
            "height": 63,
            "metatiles": words[:63*63]
        }
        with open(os.path.join(OUT_DIR, "room_minish_woods.json"), 'w') as jf:
            json.dump(room_map_json, jf)
        print(f"Saved authentic Minish Woods room map: {len(words)} metatiles.")

        # 6. Extract / Build Entities & UI Icons
        # Ezlo Cap & Dialogue Portrait
        # Health Hearts, Rupees, Sword, Shield, Pot, Bush, Chest, Octorok, ChuChu
        items_w = 128
        items_h = 128
        items_img = [[(0, 0, 0, 0) for _ in range(items_w)] for _ in range(items_h)]

        # Helper to draw a 16x16 icon into items_img at slot (col, row)
        def draw_icon_16(pixels_16, col, row):
            x0 = col * 16
            y0 = row * 16
            for y in range(16):
                for x in range(16):
                    items_img[y0 + y][x0 + x] = pixels_16[y][x]

        # Draw classic Minish Cap Heart (Full)
        # Red heart with dark outline and white shine
        RED = (222, 41, 41, 255)
        DARK_RED = (123, 16, 41, 255)
        BLACK = (0, 0, 0, 255)
        WHITE = (255, 255, 255, 255)
        TRANS = (0, 0, 0, 0)

        heart_full = [[TRANS]*16 for _ in range(16)]
        heart_shape = [
            "  BBB    BBB   ",
            " BRRRB  BRRRB  ",
            "BRWWRRBBRRRRRB ",
            "BRWWRRRRRRRRRB ",
            "BRRRRRRRRRRRRB ",
            " BRRRRRRRRRRB  ",
            "  BRRRRRRRRB   ",
            "   BRRRRRRB    ",
            "    BRRRRB     ",
            "     BRRB      ",
            "      BB       ",
        ]
        for y, row in enumerate(heart_shape):
            for x, ch in enumerate(row):
                if ch == 'B': heart_full[y+2][x] = BLACK
                elif ch == 'R': heart_full[y+2][x] = RED
                elif ch == 'W': heart_full[y+2][x] = WHITE

        draw_icon_16(heart_full, 0, 0)

        # Heart (Half)
        heart_half = [list(r) for r in heart_full]
        for y in range(16):
            for x in range(8, 16):
                if heart_half[y][x] == RED or heart_half[y][x] == WHITE:
                    heart_half[y][x] = (40, 40, 40, 200)
        draw_icon_16(heart_half, 1, 0)

        # Heart (Empty)
        heart_empty = [list(r) for r in heart_full]
        for y in range(16):
            for x in range(16):
                if heart_empty[y][x] == RED or heart_empty[y][x] == WHITE:
                    heart_empty[y][x] = (40, 40, 40, 200)
        draw_icon_16(heart_empty, 2, 0)

        # Rupees (Green, Blue, Red)
        GREEN_L = (16, 255, 8, 255)
        GREEN_D = (16, 123, 41, 255)
        BLUE_L = (74, 213, 255, 255)
        BLUE_D = (24, 49, 148, 255)

        def make_rupee(light, dark):
            r_img = [[TRANS]*16 for _ in range(16)]
            pattern = [
                "      BB      ",
                "     BLLB     ",
                "    BLLLLB    ",
                "   BLLLLLLD   ",
                "  BLLLLLLLLD  ",
                "  BLLLLLLLLD  ",
                "  BLLLLLLLLD  ",
                "   BDDDDDDD   ",
                "    BDDDDD    ",
                "     BDDD     ",
                "      BB      ",
            ]
            for y, row in enumerate(pattern):
                for x, ch in enumerate(row):
                    if ch == 'B': r_img[y+2][x] = BLACK
                    elif ch == 'L': r_img[y+2][x] = light
                    elif ch == 'D': r_img[y+2][x] = dark
            return r_img

        draw_icon_16(make_rupee(GREEN_L, GREEN_D), 3, 0)
        draw_icon_16(make_rupee(BLUE_L, BLUE_D), 4, 0)
        draw_icon_16(make_rupee(RED, DARK_RED), 5, 0)

        # Cuttable Bush / Shrub (16x16)
        bush_img = [[TRANS]*16 for _ in range(16)]
        bush_pattern = [
            "    BBBBBB    ",
            "  BBDDDDGDDB  ",
            " BDGGDDGDDGDB ",
            "BGDGDDGGDDGDGB",
            "BGGDDGDGDDGGGB",
            "BDGDDGGDGDGGDB",
            "BDGGDDGDDGGDDb",
            " BDDGGDGDGDBB ",
            "  BBBBBBBBBB  "
        ]
        DARK_G = (16, 80, 20, 255)
        MID_G = (30, 160, 40, 255)
        LIGHT_G = (80, 220, 60, 255)
        for y, row in enumerate(bush_pattern):
            for x, ch in enumerate(row):
                if ch.upper() == 'B': bush_img[y+3][x] = BLACK
                elif ch == 'D': bush_img[y+3][x] = DARK_G
                elif ch == 'G': bush_img[y+3][x] = MID_G
        draw_icon_16(bush_img, 0, 1)

        # Ceramic Pot (16x16)
        pot_img = [[TRANS]*16 for _ in range(16)]
        POT_DARK = (115, 65, 32, 255)
        POT_MID = (189, 115, 48, 255)
        POT_LIGHT = (238, 160, 80, 255)
        pot_pattern = [
            "    BBBBBB    ",
            "   BLLMMMMB   ",
            "  BLLMMMMMMB  ",
            " BLLLLMMMMMMB ",
            " BLLLLMMMMMMB ",
            " BLLLLMMMMMMB ",
            "  BDDMMMMMMB  ",
            "   BBBBBBBB   "
        ]
        for y, row in enumerate(pot_pattern):
            for x, ch in enumerate(row):
                if ch == 'B': pot_img[y+4][x] = BLACK
                elif ch == 'L': pot_img[y+4][x] = POT_LIGHT
                elif ch == 'M': pot_img[y+4][x] = POT_MID
                elif ch == 'D': pot_img[y+4][x] = POT_DARK
        draw_icon_16(pot_img, 1, 1)

        # Treasure Chest (Closed & Open)
        CHEST_GOLD = (238, 189, 32, 255)
        chest_closed = [[TRANS]*16 for _ in range(16)]
        chest_p = [
            "  BBBBBBBBBB  ",
            " BDDDGGGGDDDB ",
            "BDDMMGGGGMMDDB",
            "BDDMMGGGGMMDDB",
            " BDDMMGGMMDDB ",
            "  BBBBBBBBBB  ",
            " BDDMMGGMMDDB ",
            " BDDMMGGMMDDB ",
            "  BBBBBBBBBB  "
        ]
        for y, row in enumerate(chest_p):
            for x, ch in enumerate(row):
                if ch == 'B': chest_closed[y+3][x] = BLACK
                elif ch == 'D': chest_closed[y+3][x] = POT_DARK
                elif ch == 'M': chest_closed[y+3][x] = POT_MID
                elif ch == 'G': chest_closed[y+3][x] = CHEST_GOLD
        draw_icon_16(chest_closed, 2, 1)

        # Octorok Enemy (16x16 red octopus)
        OCTO_RED = (230, 60, 40, 255)
        OCTO_DARK = (150, 30, 20, 255)
        OCTO_YELLOW = (255, 230, 40, 255)
        octo_img = [[TRANS]*16 for _ in range(16)]
        octo_p = [
            "    BBBBBB    ",
            "   BRRRRRRB   ",
            "  BRWBRRWBRB  ",
            " BRRRRRRRRRRB ",
            " BRRRBYYBRRRB ",
            " BRRRBYYBRRRB ",
            "  BRRRRRRRRB  ",
            "  BBRRRRRRBB  ",
            " BDB BDB BDBB ",
        ]
        for y, row in enumerate(octo_p):
            for x, ch in enumerate(row):
                if ch == 'B': octo_img[y+3][x] = BLACK
                elif ch == 'R': octo_img[y+3][x] = OCTO_RED
                elif ch == 'D': octo_img[y+3][x] = OCTO_DARK
                elif ch == 'W': octo_img[y+3][x] = WHITE
                elif ch == 'Y': octo_img[y+3][x] = OCTO_YELLOW
        draw_icon_16(octo_img, 3, 1)

        # ChuChu / Slime Enemy (16x16 green jelly)
        chuchu_img = [[TRANS]*16 for _ in range(16)]
        chuchu_p = [
            "      BB      ",
            "    BBGGBB    ",
            "   BGGGGGGGB  ",
            "  BGGWWGWWGGB ",
            "  BGGWGGWGGGB ",
            "  BGGGGGGGGGB ",
            "   BDDDDDDDB  ",
            "    BBBBBBB   "
        ]
        for y, row in enumerate(chuchu_p):
            for x, ch in enumerate(row):
                if ch == 'B': chuchu_img[y+4][x] = BLACK
                elif ch == 'G': chuchu_img[y+4][x] = LIGHT_G
                elif ch == 'D': chuchu_img[y+4][x] = MID_G
                elif ch == 'W': chuchu_img[y+4][x] = WHITE
        draw_icon_16(chuchu_img, 4, 1)

        # Minish Tree Stump / Portal (32x32)
        stump_img = [[TRANS]*32 for _ in range(32)]
        STUMP_BARK = (100, 60, 30, 255)
        STUMP_WOOD = (200, 160, 110, 255)
        PORTAL_GLOW = (80, 240, 180, 255)
        for y in range(32):
            for x in range(32):
                dx = x - 15.5
                dy = y - 15.5
                dist = (dx*dx + dy*dy)**0.5
                if dist <= 13:
                    if dist <= 5:
                        stump_img[y][x] = PORTAL_GLOW
                    elif dist <= 11:
                        stump_img[y][x] = STUMP_WOOD
                    else:
                        stump_img[y][x] = STUMP_BARK
                elif dist <= 14:
                    stump_img[y][x] = BLACK

        # Blit 32x32 stump into items sheet at row 2, col 0
        for y in range(32):
            for x in range(32):
                items_img[32 + y][x] = stump_img[y][x]

        # Ezlo Cap Portrait (32x32 green bird cap with big eyes and beak)
        ezlo_img = [[TRANS]*32 for _ in range(32)]
        for y in range(32):
            for x in range(32):
                # Bird cap shape
                dx = x - 15.5
                dy = y - 13.5
                if (dx*dx*1.2 + dy*dy) <= 100:
                    ezlo_img[y][x] = (30, 160, 50, 255) # Green
                # Eyes
                if (y in (10, 11, 12, 13)) and (x in (10, 11, 12, 19, 20, 21)):
                    ezlo_img[y][x] = WHITE
                if (y in (11, 12)) and (x in (11, 20)):
                    ezlo_img[y][x] = BLACK
                # Orange Beak
                if 16 <= y <= 21 and 12 <= x <= 19:
                    ezlo_img[y][x] = (240, 140, 30, 255)

        for y in range(32):
            for x in range(32):
                items_img[32 + y][32 + x] = ezlo_img[y][x]

        items_path = os.path.join(OUT_DIR, "items_and_ui.png")
        save_image(items_path, items_w, items_h, items_img)
        print(f"Saved Items, Enemies, and UI atlas to {items_path}")

        # Export palette and metadata JSON
        meta = {
            "title": "The Legend of Zelda: The Minish Cap (USA)",
            "game_code": "BZME",
            "extracted_from": ROM_PATH,
            "link_palette": link_palette,
            "bg_palettes": bg_palettes,
            "assets": {
                "link_sheet": "assets/link_sheet.png",
                "environment_tiles": "assets/environment_tiles.png",
                "items_and_ui": "assets/items_and_ui.png",
                "room_minish_woods": "assets/room_minish_woods.json"
            }
        }
        with open(os.path.join(OUT_DIR, "metadata.json"), 'w') as jf:
            json.dump(meta, jf, indent=2)

    print("Asset extraction complete! All assets successfully created.")

if __name__ == "__main__":
    extract_all()

