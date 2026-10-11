#import <Foundation/Foundation.h>
#import <JavaScriptCore/JavaScriptCore.h>
#include "gba_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach_time.h>

static JSGlobalContextRef s_ctx = NULL;
static JSObjectRef s_stepFunc = NULL;
static JSObjectRef s_currentFramePixels = NULL;
static u32* s_framePtr = NULL;
static mach_timebase_info_data_t s_timebase = {0, 0};

static void* read_file_any(const char* rel_path, size_t* out_len) {
    FILE* f = fopen(rel_path, "rb");
    if (!f) {
        char alt_path[1024];
        snprintf(alt_path, sizeof(alt_path), "../%s", rel_path);
        f = fopen(alt_path, "rb");
    }
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    void* buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    ((char*)buf)[sz] = '\0';
    fclose(f);

    if (out_len) *out_len = (size_t)sz;
    return buf;
}

bool gba_bridge_init(const char* rom_path, const char* bios_path) {
    printf("[GBA Bridge] Initializing authentic ARM7TDMI bytecode engine...\n");
    mach_timebase_info(&s_timebase);

    s_ctx = JSGlobalContextCreate(NULL);
    JSObjectRef global = JSContextGetGlobalObject(s_ctx);

    // Browser environment shims for GBA core with zero-copy frame buffer
    const char* shim = 
        "var window = { setTimeout: function(){}, clearTimeout: function(){}, addEventListener: function(){} };"
        "var currentFramePixels = null;"
        "var document = {"
        "  createElement: function() {"
        "    return {"
        "      getContext: function() {"
        "        return {"
        "          createImageData: function(w, h) {"
        "            var data = new Uint8ClampedArray(w * h * 4);"
        "            currentFramePixels = new Uint32Array(data.buffer, data.byteOffset, w * h);"
        "            return { data: data };"
        "          },"
        "          putImageData: function(imgData, x, y) {}"
        "        };"
        "      }"
        "    };"
        "  }"
        "};"
        "var navigator = { userAgent: 'macOS Native' };"
        "var console = { log: function(m){}, warn: function(m){}, error: function(m){} };";

    JSStringRef shim_str = JSStringCreateWithUTF8CString(shim);
    JSEvaluateScript(s_ctx, shim_str, NULL, NULL, 1, NULL);
    JSStringRelease(shim_str);

    // Load Core JavaScript Engine Modules
    const char* files[] = {
        "gba_core/js/util.js", "gba_core/js/core.js", "gba_core/js/arm.js", "gba_core/js/thumb.js",
        "gba_core/js/mmu.js", "gba_core/js/io.js", "gba_core/js/audio.js", "gba_core/js/video.js",
        "gba_core/js/video/proxy.js", "gba_core/js/video/software.js", "gba_core/js/irq.js",
        "gba_core/js/keypad.js", "gba_core/js/sio.js", "gba_core/js/gpio.js", "gba_core/js/savedata.js",
        "gba_core/js/gba.js"
    };

    for (int i = 0; i < 16; i++) {
        size_t len;
        char* code = (char*)read_file_any(files[i], &len);
        if (!code) {
            fprintf(stderr, "[GBA Bridge] Fatal: Could not read core engine file: %s\n", files[i]);
            return false;
        }
        JSStringRef script = JSStringCreateWithUTF8CString(code);
        free(code);
        JSEvaluateScript(s_ctx, script, NULL, NULL, 1, NULL);
        JSStringRelease(script);
    }

    // Load BIOS and ROM binaries
    size_t bios_len = 0;
    void* bios_data = read_file_any(bios_path ? bios_path : "gba_core/resources/bios.bin", &bios_len);

    size_t rom_len = 0;
    void* rom_data = read_file_any(rom_path ? rom_path : "Legend of Zelda, The - The Minish Cap (USA).gba", &rom_len);

    if (!rom_data) {
        fprintf(stderr, "[GBA Bridge] Fatal: Could not load ROM binary: %s\n", rom_path);
        return false;
    }

    JSObjectRef rom_ab = JSObjectMakeArrayBufferWithBytesNoCopy(s_ctx, rom_data, rom_len, NULL, NULL, NULL);
    JSStringRef rom_key = JSStringCreateWithUTF8CString("rawRom");
    JSObjectSetProperty(s_ctx, global, rom_key, rom_ab, 0, NULL);
    JSStringRelease(rom_key);

    if (bios_data) {
        JSObjectRef bios_ab = JSObjectMakeArrayBufferWithBytesNoCopy(s_ctx, bios_data, bios_len, NULL, NULL, NULL);
        JSStringRef bios_key = JSStringCreateWithUTF8CString("rawBios");
        JSObjectSetProperty(s_ctx, global, bios_key, bios_ab, 0, NULL);
        JSStringRelease(bios_key);
    }

    // Bootstrap GBA Core with performance optimizations:
    // 1. Precomputed 16-to-32 color LUT table
    // 2. Direct 32-bit typed array scanline rasterization
    // 3. Fast inlined step loop
    // 4. Inlined CPU IRQ update timer checks (eliminating 3M calls/sec)
    // 5. High-speed selective scanline rendering
    const char* boot_code =
        "var gba = new GameBoyAdvance();"
        "var fakeCanvas = document.createElement();"
        "gba.setCanvas(fakeCanvas);"
        "if (typeof rawBios !== 'undefined') gba.setBios(rawBios);"
        "gba.setRom(rawRom);"
        ""
        "var colorLUT = new Uint32Array(65536);"
        "for (var i = 0; i < 65536; i++) {"
        "  var r = (i & 0x001F) << 3;"
        "  var g = (i & 0x03E0) >> 2;"
        "  var b = (i & 0x7C00) >> 7;"
        "  colorLUT[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;"
        "}"
        ""
        "var renderer = gba.video.renderPath;"
        "renderer.finishScanline = function(backing) {"
        "  var bd = this.palette.accessColor(this.LAYER_BACKDROP, 0);"
        "  var bdPix = colorLUT[bd & 0xFFFF];"
        "  var rowOffset = this.vcount * 240;"
        "  var isTarget2 = this.target2[this.LAYER_BACKDROP];"
        "  var stencil = backing.stencil;"
        "  var color = backing.color;"
        "  var wmask = this.WRITTEN_MASK;"
        "  var t1mask = this.TARGET1_MASK;"
        "  for (var x = 0; x < 240; ++x) {"
        "    if (stencil[x] & wmask) {"
        "      var c = color[x];"
        "      if (isTarget2 && (stencil[x] & t1mask)) {"
        "        c = this.palette.mix(this.blendA, c, this.blendB, bd);"
        "      }"
        "      currentFramePixels[rowOffset + x] = colorLUT[c & 0xFFFF];"
        "    } else {"
        "      currentFramePixels[rowOffset + x] = bdPix;"
        "    }"
        "  }"
        "};"
        ""
        "gba.step = function() {"
        "  var cpu = this.cpu;"
        "  while (!this.seenFrame) {"
        "    cpu.step();"
        "  }"
        "  this.seenFrame = false;"
        "};"
        ""
        "var cpu = gba.cpu;"
        "var gprs = cpu.gprs;"
        "var mmu = cpu.mmu;"
        "var irq = cpu.irq;"
        "cpu.step = function() {"
        "  var instruction = this.instruction || (this.instruction = this.loadInstruction(gprs[this.PC] - this.instructionWidth));"
        "  gprs[this.PC] += this.instructionWidth;"
        "  this.conditionPassed = true;"
        "  instruction();"
        "  if (!instruction.writesPC) {"
        "    if (this.instruction != null) {"
        "      if (instruction.next == null || instruction.next.page.invalid) {"
        "        instruction.next = this.loadInstruction(gprs[this.PC] - this.instructionWidth);"
        "      }"
        "      this.instruction = instruction.next;"
        "    }"
        "  } else {"
        "    if (this.conditionPassed) {"
        "      var pc = gprs[this.PC] &= 0xFFFFFFFE;"
        "      if (this.execMode == this.MODE_ARM) {"
        "        mmu.wait32(pc);"
        "        mmu.waitPrefetch32(pc);"
        "      } else {"
        "        mmu.wait(pc);"
        "        mmu.waitPrefetch(pc);"
        "      }"
        "      gprs[this.PC] += this.instructionWidth;"
        "      if (!instruction.fixedJump) {"
        "        this.instruction = null;"
        "      } else if (this.instruction != null) {"
        "        if (instruction.next == null || instruction.next.page.invalid) {"
        "          instruction.next = this.loadInstruction(gprs[this.PC] - this.instructionWidth);"
        "        }"
        "        this.instruction = instruction.next;"
        "      }"
        "    } else {"
        "      this.instruction = null;"
        "    }"
        "  }"
        "  if (irq.nextEvent <= this.cycles) {"
        "    irq.updateTimers();"
        "  }"
        "};"
        ""
        "var origScanline = gba.video.renderPath.drawScanline;"
        "var noop = function(){};"
        "var stepFunction = function(keyVal, skip) {"
        "  gba.keypad.currentDown = keyVal;"
        "  if (skip) {"
        "    gba.video.renderPath.drawScanline = noop;"
        "    gba.step();"
        "    gba.video.renderPath.drawScanline = origScanline;"
        "  } else {"
        "    gba.step();"
        "  }"
        "};";

    JSStringRef boot_str = JSStringCreateWithUTF8CString(boot_code);
    JSEvaluateScript(s_ctx, boot_str, NULL, NULL, 1, NULL);
    JSStringRelease(boot_str);

    // Cache stepFunction object for zero-overhead invocation
    JSStringRef step_name = JSStringCreateWithUTF8CString("stepFunction");
    JSValueRef step_val = JSObjectGetProperty(s_ctx, global, step_name, NULL);
    s_stepFunc = JSValueToObject(s_ctx, step_val, NULL);
    JSStringRelease(step_name);

    // Cache direct pointer to PPU framebuffer
    JSStringRef pix_name = JSStringCreateWithUTF8CString("currentFramePixels");
    JSValueRef pix_val = JSObjectGetProperty(s_ctx, global, pix_name, NULL);
    s_currentFramePixels = JSValueToObject(s_ctx, pix_val, NULL);
    s_framePtr = (u32*)JSObjectGetTypedArrayBytesPtr(s_ctx, s_currentFramePixels, NULL);
    JSStringRelease(pix_name);

    printf("[GBA Bridge] Authentic Minish Cap ROM engine loaded successfully! Ready for Metal 60 FPS.\n");
    return true;
}

void gba_bridge_step_ex(u32* out_pixels, u16 key_state, bool skip_render) {
    if (!s_ctx || !s_stepFunc) return;

    // 1. Step ARM7TDMI CPU cycles with active-low keypad state and optional scanline skip
    JSValueRef args[2] = {
        JSValueMakeNumber(s_ctx, (double)key_state),
        JSValueMakeBoolean(s_ctx, skip_render)
    };
    JSObjectCallAsFunction(s_ctx, s_stepFunc, NULL, 2, args, NULL);

    // 2. Extract 240x160 RGBA pixels directly from PPU backing buffer if rendered
    if (s_framePtr && out_pixels && !skip_render) {
        memcpy(out_pixels, s_framePtr, 240 * 160 * sizeof(u32));
    }
}

void gba_bridge_step(u32* out_pixels, u16 key_state) {
    gba_bridge_step_ex(out_pixels, key_state, false);
}

void gba_bridge_free(void) {
    if (s_ctx) {
        JSGlobalContextRelease(s_ctx);
        s_ctx = NULL;
    }
    printf("[GBA Bridge] Cleaned up engine.\n");
}
