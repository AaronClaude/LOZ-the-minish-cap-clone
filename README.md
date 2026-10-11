# The Legend of Zelda: The Minish Cap — Dual Engine & Reverse-Engineering Project

An authentic recreation and reverse-engineering project of **The Legend of Zelda: The Minish Cap** (Game Boy Advance, 2004, Capcom / Nintendo).

This repository features **two distinct implementations** designed for different runtime environments:

1. **Native macOS Engine (`native/`)**: High-performance desktop application written in **C / Objective-C** using Apple's **Metal 3** graphics framework and hardware JIT execution.
2. **Web Edition (`index.html`)**: Zero-install, browser-based version powered by standard **HTML5 Canvas**, modern CSS, and the **Web Audio API**.

---

## Architecture Overview: Two Separate Systems

| Feature | Native macOS Engine | Web Edition |
| :--- | :--- | :--- |
| **Language & Stack** | C, Objective-C, Metal Shading Language (MSL) | HTML5, JavaScript (ES6+), Web Audio API |
| **Target Platform** | macOS 12+ (Apple Silicon M1/M2/M3/M4 & Intel) | Any modern desktop/mobile web browser |
| **Graphics API** | **Apple Metal 3 (`MTLGPUFamilyMetal3`)** | HTML5 2D Canvas context |
| **PPU Rendering** | Custom Metal vertex/fragment shaders with nearest-neighbor retro sampling | 240×160 native Canvas with CSS integer scaling |
| **Simulation Speed** | Locked **60.0 FPS (100% Real-Time Speed)** via JIT entitlement | Real-time 60 FPS in-browser loop |
| **Audio Subsystem** | Standalone emulation core / CoreAudio ready | Web Audio API with real-time waveform synthesis |
| **Special Features** | Live FPS & Speed HUD badge, <kbd>Esc</kbd> Pause overlay | CRT Scanline retro filter, Fullscreen toggle, Help modal |

---

## 1. Native macOS Engine (C / Objective-C / Metal 3)

The native engine is located in the [`native/`](native/) directory and is built specifically for macOS.

### Key Highlights
- **Hardware Acceleration**: Built directly on Apple's **MetalKit** and **Metal 3** framework (`MTLGPUFamilyMetal3`). Shaders run directly on the Apple Silicon GPU with integer-ratio aspect correction (3:2 letterbox/pillarbox).
- **JIT-Accelerated Bytecode Execution**: Uses an ad-hoc signed entitlement (`com.apple.security.cs.allow-jit`) to unlock JavaScriptCore's high-performance JIT compiler on Apple Silicon. Emulation frames execute in **~3.5 ms** (well within the 16.66 ms budget), guaranteeing a locked **60.0 FPS at 100% real-time speed**.
- **Real-Time HUD Counter**: Displays GPU presentation rate alongside actual in-game simulation speed (e.g., `FPS: 60 (100% Speed)`), color-coded green/yellow/red.
- **Hardware Keypad & Atomic Threading**: Keypad state is managed via lockless atomics on dedicated high-priority GCD background queues (`QOS_CLASS_USER_INTERACTIVE`), decoupled from UI event processing.
- **Reverse-Engineered C Modules**: Includes decompilation modules in [`native/src/`](native/src/) and [`native/include/`](native/include/) modeling Minish Cap's game systems:
  - `mc_player.c` / `mc_player.h`: Link's state machine, 8-directional movement, rolling, sword slashes.
  - `mc_room.c` / `mc_room.h`: Room collision maps, metatile rendering, and layer transitions.
  - `mc_entity.c` / `mc_entity.h`: Enemy behaviors (Octorok, Moblin, Keese) and item drops.
  - `mc_script.c` / `mc_script.h`: Cutscene triggers, dialogue scripting, camera panning.
  - `mc_assets.c` / `mc_assets.h`: Palette decompression, tileset management, ROM asset loading.

### How to Run the Native macOS Engine

#### Prerequisites
- macOS 12 Monterey or newer (tested on macOS Ventura, Sonoma, Sequoia).
- Xcode Command Line Tools installed (`xcode-select --install`).

#### Build and Launch
From the repository root, run:
```bash
# Build binary and start the game immediately
make run
```

Or build and run manually inside the `native/` folder:
```bash
cd native
make clean
make
./bin/minish_cap
```

---

## 2. Web Browser Edition (HTML5 / Web Audio)

The web edition is located in the repository root ([`index.html`](index.html), [`game.js`](game.js), [`style.css`](style.css)).

### Key Highlights
- **Zero Dependencies**: Pure client-side web application. No Node.js build step, bundler, or npm packages required.
- **Clean Modern Interface**: Distraction-free retro interface with a sleek dark toolbar, avoiding bulky plastic console frame mockups.
- **CRT Shader Filter**: Built-in scanline overlay toggle mimicking classic cathode-ray tube displays.
- **Web Audio API**: Audio synthesizers emulating the GBA's 2 PSG square channels, wave channel, noise channel, and direct-sound DMA channels.
- **Responsive Viewport**: Scales dynamically to fill any window while preserving the authentic 3:2 Game Boy Advance aspect ratio.

### How to Run the Web Edition

#### Prerequisites
- Python 3 (or any static HTTP web server).
- A modern web browser (Safari, Chrome, Firefox, Edge).

#### Start Web Server
From the repository root, start a local HTTP server:
```bash
python3 -m http.server 8000
```

Then open your browser and navigate to:
```
http://localhost:8000
```

---

## Controls Reference

The controls are unified across both the Native Desktop Engine and the Web Edition:

| GBA Button | Primary Keyboard Key | Secondary Key | Function in Minish Cap |
| :--- | :--- | :--- | :--- |
| **D-Pad Up** | <kbd>W</kbd> | <kbd>↑</kbd> (Up Arrow) | Move Up |
| **D-Pad Down** | <kbd>S</kbd> | <kbd>↓</kbd> (Down Arrow) | Move Down |
| **D-Pad Left** | <kbd>A</kbd> | <kbd>←</kbd> (Left Arrow) | Move Left |
| **D-Pad Right** | <kbd>D</kbd> | <kbd>→</kbd> (Right Arrow) | Move Right |
| **A Button** | <kbd>J</kbd> | <kbd>Z</kbd> / <kbd>Enter</kbd> | Sword Attack / Talk / Confirm |
| **B Button** | <kbd>K</kbd> | <kbd>X</kbd> | Roll / Cancel / Secondary Item |
| **R Button** | <kbd>E</kbd> | <kbd>R</kbd> | Grab / Push / Action / Roll |
| **L Button** | <kbd>Q</kbd> | <kbd>L</kbd> | Kinstone Fusion |
| **Start** | <kbd>Enter</kbd> | <kbd>P</kbd> | Open Quest Menu / Inventory |
| **Select** | <kbd>Shift</kbd> | <kbd>T</kbd> | Minish Cap Dialogue (Ezlo) |
| **Pause** | <kbd>Esc</kbd> *(Native only)* | — | Freeze Emulation Loop |

---

## Reverse Engineering & Asset Pipeline

This project includes custom tooling for extracting and inspecting game assets from the original GBA ROM:

### Asset Extraction (`extract_assets.py`)
Extracts Capcom LZ77-compressed assets from the ROM into structured JSON and binary files:
- **Palettes**: 16-color backdrop, tile, and sprite palettes converted from 15-bit BGR555 to RGBA.
- **Tilesets & Metatiles**: 8×8 pixel characters and 16×16 metatiles reconstructed for room rendering.
- **Maps**: Area header definitions, collision matrices, warp coordinates, and NPC placement data.

Run the asset extraction pipeline:
```bash
python3 extract_assets.py
```
Output files are generated in `assets/`, `gfx.json`, `map.json`, and `sounds.json`.

---

## Project Structure

```
├── Legend of Zelda, The - The Minish Cap (USA).gba  # GBA ROM Binary (USA: BZME)
├── Makefile                                        # Top-level build orchestrator
├── index.html                                      # Web Edition entry point
├── game.js                                         # Web client logic & UI controller
├── style.css                                       # Modern responsive dark theme
├── extract_assets.py                               # ROM asset decompressor & parser
│
├── native/                                         # Native macOS C/Metal Desktop Engine
│   ├── Makefile                                    # Clang & Metal compiler config
│   ├── minish_cap.entitlements                     # Apple Silicon JIT execution policy
│   ├── main.c                                      # Native app entry point
│   ├── include/                                    # C Header declarations
│   │   ├── platform_macos.h                        # Metal 3 window & view interface
│   │   ├── gba_bridge.h                            # ARM7TDMI bytecode bridge
│   │   ├── gba_keypad.h                            # Atomic keypad definitions
│   │   ├── gba_types.h                             # GBA integer definitions
│   │   ├── mc_player.h                             # Link movement & state machine
│   │   ├── mc_room.h                               # Room loading & collision maps
│   │   └── mc_entity.h                             # Enemy & NPC entity logic
│   └── src/                                        # Implementation sources
│       ├── platform_macos.m                        # Metal 3 pipeline & HUD overlay
│       ├── gba_bridge.m                            # JIT-backed execution bridge
│       ├── gba_keypad.c                            # Lockless atomic input handler
│       ├── mc_player.c                             # Decompiled player routines
│       ├── mc_room.c                               # Decompiled room rendering
│       └── mc_entity.c                             # Decompiled entity engine
│
└── gba_core/                                       # Core GBA Hardware Subsystem
    ├── resources/
    │   └── bios.bin                                # Authentic GBA BIOS binary
    └── js/                                         # Hardware component emulation
        ├── arm.js / thumb.js                       # ARM7TDMI instruction sets
        ├── core.js / mmu.js                        # Bus arbitration & CPU core
        ├── video/software.js                       # 4-layer PPU tile & sprite rasterizer
        └── audio.js                                # PSG & DMA sound channels
```

---

## Disclaimer & Fair Use

This repository is strictly for educational, reverse-engineering, and research purposes. All trademarks, game assets, storylines, and characters belong to **Nintendo Co., Ltd.** and **Capcom Co., Ltd.**.

