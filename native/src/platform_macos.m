#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include <os/lock.h>
#include <mach/mach_time.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include "platform_macos.h"

// Metal Shading Language source: Nearest-neighbor fullscreen textured quad
static const char* kMetalShaderSource =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "\n"
    "struct VertexOut {\n"
    "    float4 position [[position]];\n"
    "    float2 texCoord;\n"
    "};\n"
    "\n"
    "vertex VertexOut gba_vertex(uint vid [[vertex_id]]) {\n"
    "    // Triangle strip quad coordinates: (-1, 1), (-1, -1), (1, 1), (1, -1)\n"
    "    const float2 pos[4] = { float2(-1.0,  1.0), float2(-1.0, -1.0), float2( 1.0,  1.0), float2( 1.0, -1.0) };\n"
    "    const float2 uv[4]  = { float2( 0.0,  0.0), float2( 0.0,  1.0), float2( 1.0,  0.0), float2( 1.0,  1.0) };\n"
    "    VertexOut out;\n"
    "    out.position = float4(pos[vid], 0.0, 1.0);\n"
    "    out.texCoord = uv[vid];\n"
    "    return out;\n"
    "}\n"
    "\n"
    "fragment float4 gba_fragment(VertexOut in [[stage_in]],\n"
    "                             texture2d<float> tex [[texture(0)]]) {\n"
    "    constexpr sampler s(filter::nearest, address::clamp_to_edge);\n"
    "    return tex.sample(s, in.texCoord);\n"
    "}\n";

@interface GbaMetalView : MTKView <MTKViewDelegate> {
    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    id<MTLRenderPipelineState> _pipelineState;
    id<MTLTexture> _texture;

    // Double-buffered emulation framebuffer
    u32 _frontBuffer[GBA_PIXEL_COUNT];
    u32 _backBuffer[GBA_PIXEL_COUNT];
    os_unfair_lock _bufferLock;
    BOOL _newFrameAvailable;

    // Controls & Callback
    GbaKeypad _keypad;
    void (*_frameCallback)(void*, u32*, u16, bool);
    void* _userData;

    // Dedicated Emulation Thread
    dispatch_queue_t _emuQueue;
    _Atomic bool _emuRunning;
    _Atomic bool _isPaused;

    // UI Overlays
    NSTextField* _fpsLabel;
    NSTextField* _pauseLabel;

    // Performance profiling (Presentation FPS & In-Game Simulation Speed)
    uint64_t _lastFpsTime;
    uint32_t _renderedFrames;
    uint32_t _simulatedFrames;
    mach_timebase_info_data_t _timebase;
}

- (instancetype)initWithFrame:(NSRect)frameRect
                     callback:(void (*)(void*, u32*, u16, bool))cb
                     userData:(void*)ud;
- (void)stopEmulation;
- (void)togglePause;
@end

@implementation GbaMetalView

- (instancetype)initWithFrame:(NSRect)frameRect
                     callback:(void (*)(void*, u32*, u16, bool))cb
                     userData:(void*)ud {
    // 1. Initialize system default Apple Metal device
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
        fprintf(stderr, "[Fatal] Apple Metal is not supported on this Mac hardware!\n");
        return nil;
    }

    self = [super initWithFrame:frameRect device:device];
    if (self) {
        _device = device;
        _frameCallback = cb;
        _userData = ud;
        _bufferLock = OS_UNFAIR_LOCK_INIT;
        _newFrameAvailable = NO;
        atomic_init(&_emuRunning, true);
        atomic_init(&_isPaused, false);
        _renderedFrames = 0;
        _simulatedFrames = 0;
        mach_timebase_info(&_timebase);
        _lastFpsTime = mach_absolute_time();

        keypad_init(&_keypad);
        memset(_frontBuffer, 0, sizeof(_frontBuffer));
        memset(_backBuffer, 0, sizeof(_backBuffer));

        // Detect and log Metal tier support
        if (@available(macOS 13.0, *)) {
            if ([_device supportsFamily:MTLGPUFamilyMetal3]) {
                printf("[Metal] Hardware Acceleration: Tier 3 Metal (MTLGPUFamilyMetal3) active on %s\n", [[_device name] UTF8String]);
            } else {
                printf("[Metal] Hardware Acceleration: Apple GPU Family active on %s\n", [[_device name] UTF8String]);
            }
        } else {
            printf("[Metal] Hardware Acceleration: Metal GPU active on %s\n", [[_device name] UTF8String]);
        }

        // Configure MTKView properties
        self.delegate = self;
        self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        self.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
        self.preferredFramesPerSecond = 60;
        self.enableSetNeedsDisplay = NO;
        self.paused = YES;

        // 2. Create Metal Command Queue
        _commandQueue = [_device newCommandQueue];

        // 3. Compile MSL Shader Pipeline
        NSError* error = nil;
        id<MTLLibrary> library = [_device newLibraryWithSource:[NSString stringWithUTF8String:kMetalShaderSource]
                                                       options:nil
                                                         error:&error];
        if (!library) {
            fprintf(stderr, "[Metal] Error compiling shader library: %s\n", [[error localizedDescription] UTF8String]);
            return nil;
        }

        id<MTLFunction> vertFunc = [library newFunctionWithName:@"gba_vertex"];
        id<MTLFunction> fragFunc = [library newFunctionWithName:@"gba_fragment"];

        MTLRenderPipelineDescriptor* pipeDesc = [[MTLRenderPipelineDescriptor alloc] init];
        pipeDesc.vertexFunction = vertFunc;
        pipeDesc.fragmentFunction = fragFunc;
        pipeDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        _pipelineState = [_device newRenderPipelineStateWithDescriptor:pipeDesc error:&error];
        if (!_pipelineState) {
            fprintf(stderr, "[Metal] Error creating pipeline state: %s\n", [[error localizedDescription] UTF8String]);
            return nil;
        }

        // 4. Create 240x160 RGBA8Unorm Texture for GBA Display
        MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                           width:GBA_SCREEN_WIDTH
                                                                                          height:GBA_SCREEN_HEIGHT
                                                                                       mipmapped:NO];
        texDesc.usage = MTLTextureUsageShaderRead;
        texDesc.storageMode = MTLStorageModeShared;
        _texture = [_device newTextureWithDescriptor:texDesc];

        // 5. Setup UI Overlays (FPS & Game Speed Badge, Pause Overlay)
        _fpsLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(frameRect.size.width - 160, frameRect.size.height - 32, 150, 24)];
        [_fpsLabel setBezeled:NO];
        [_fpsLabel setDrawsBackground:YES];
        [_fpsLabel setBackgroundColor:[NSColor colorWithCalibratedRed:0 green:0 blue:0 alpha:0.75]];
        [_fpsLabel setTextColor:[NSColor colorWithCalibratedRed:0.2 green:1.0 blue:0.4 alpha:1.0]];
        [_fpsLabel setFont:[NSFont monospacedDigitSystemFontOfSize:11.0 weight:NSFontWeightBold]];
        [_fpsLabel setAlignment:NSTextAlignmentCenter];
        [_fpsLabel setStringValue:@"FPS: 60 (100% Speed)"];
        [_fpsLabel setEditable:NO];
        [_fpsLabel setSelectable:NO];
        [_fpsLabel setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
        _fpsLabel.wantsLayer = YES;
        _fpsLabel.layer.cornerRadius = 5.0;
        _fpsLabel.layer.masksToBounds = YES;
        [self addSubview:_fpsLabel];

        // Centered [ PAUSED ] Overlay
        _pauseLabel = [[NSTextField alloc] initWithFrame:NSMakeRect((frameRect.size.width - 200) / 2, (frameRect.size.height - 44) / 2, 200, 44)];
        [_pauseLabel setBezeled:NO];
        [_pauseLabel setDrawsBackground:YES];
        [_pauseLabel setBackgroundColor:[NSColor colorWithCalibratedRed:0 green:0 blue:0 alpha:0.85]];
        [_pauseLabel setTextColor:[NSColor colorWithCalibratedRed:1.0 green:0.8 blue:0.1 alpha:1.0]];
        [_pauseLabel setFont:[NSFont boldSystemFontOfSize:18.0]];
        [_pauseLabel setAlignment:NSTextAlignmentCenter];
        [_pauseLabel setStringValue:@"[ PAUSED ]"];
        [_pauseLabel setEditable:NO];
        [_pauseLabel setSelectable:NO];
        [_pauseLabel setHidden:YES];
        [_pauseLabel setAutoresizingMask:NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin | NSViewMaxYMargin];
        _pauseLabel.wantsLayer = YES;
        _pauseLabel.layer.cornerRadius = 8.0;
        _pauseLabel.layer.borderColor = [NSColor colorWithCalibratedRed:1.0 green:0.8 blue:0.1 alpha:0.6].CGColor;
        _pauseLabel.layer.borderWidth = 2.0;
        _pauseLabel.layer.masksToBounds = YES;
        [self addSubview:_pauseLabel];

        // 6. Decouple Emulation Loop to Dedicated High-Priority Background GCD Queue
        _emuQueue = dispatch_queue_create("com.zelda.emu", DISPATCH_QUEUE_SERIAL);
        dispatch_set_target_queue(_emuQueue, dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0));
        [self startEmulationThread];
    }
    return self;
}

- (void)startEmulationThread {
    dispatch_async(_emuQueue, ^{
        mach_timebase_info_data_t tb;
        mach_timebase_info(&tb);
        const uint64_t targetFrameNanos = 1000000000ULL / 60; // 16.666 ms per GBA frame
        uint64_t nextFrameTime = mach_absolute_time() + (targetFrameNanos * tb.denom / tb.numer);

        while (atomic_load(&self->_emuRunning)) {
            @autoreleasepool {
                if (atomic_load(&self->_isPaused)) {
                    usleep(16000);
                    nextFrameTime = mach_absolute_time() + (targetFrameNanos * tb.denom / tb.numer);
                    continue;
                }

                uint64_t now = mach_absolute_time();
                const uint64_t frameTicks = targetFrameNanos * tb.denom / tb.numer;

                // Render every frame. Only skip rasterization as a last-resort safety
                // valve when more than 2 frames behind, and never twice in a row,
                // so animation never turns choppy.
                static bool skippedLast = false;
                bool skipRender = !skippedLast && (now > nextFrameTime + 2 * frameTicks);
                skippedLast = skipRender;

                u16 keys = keypad_get_state(&self->_keypad);
                if (self->_frameCallback) {
                    self->_frameCallback(self->_userData, self->_backBuffer, keys, skipRender);
                }
                self->_simulatedFrames++;

                // If this frame was rasterized, upload to Metal presentation buffer
                if (!skipRender) {
                    os_unfair_lock_lock(&self->_bufferLock);
                    memcpy(self->_frontBuffer, self->_backBuffer, sizeof(self->_frontBuffer));
                    self->_newFrameAvailable = YES;
                    os_unfair_lock_unlock(&self->_bufferLock);
                }

                nextFrameTime += frameTicks;

                // Prevent backlog build-up during window dragging or resizing
                now = mach_absolute_time();
                uint64_t maxLag = (uint64_t)(0.1 * 1000000000.0 * tb.denom / tb.numer);
                if (now > nextFrameTime + maxLag) {
                    nextFrameTime = now;
                }

                // If ahead of schedule, sleep precisely until nextFrameTime
                if (now < nextFrameTime) {
                    uint64_t sleepNanos = (nextFrameTime - now) * tb.numer / tb.denom;
                    if (sleepNanos > 1500000) {
                        usleep((useconds_t)((sleepNanos - 1000000) / 1000));
                    }
                    while (mach_absolute_time() < nextFrameTime) {
                        // spinwait for sub-millisecond precision
                    }
                }
            }
        }
    });
}

- (void)stopEmulation {
    atomic_store(&_emuRunning, false);
}

- (void)togglePause {
    bool paused = !atomic_load(&_isPaused);
    atomic_store(&_isPaused, paused);
    [_pauseLabel setHidden:!paused];
    printf("[System] %s\n", paused ? "Game PAUSED (Press ESC to Resume)" : "Game RESUMED");
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

#pragma mark - MTKViewDelegate

- (void)drawInMTKView:(MTKView *)view {
    // 1. If a new frame was rendered by the background emulation thread, upload to Metal texture
    os_unfair_lock_lock(&_bufferLock);
    if (_newFrameAvailable) {
        MTLRegion region = MTLRegionMake2D(0, 0, GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT);
        [_texture replaceRegion:region
                    mipmapLevel:0
                      withBytes:_frontBuffer
                    bytesPerRow:GBA_SCREEN_WIDTH * sizeof(u32)];
        _newFrameAvailable = NO;
    }
    os_unfair_lock_unlock(&_bufferLock);

    // 2. Hardware-accelerated GPU render pass
    id<CAMetalDrawable> drawable = [view currentDrawable];
    MTLRenderPassDescriptor* rpd = [view currentRenderPassDescriptor];
    if (drawable && rpd) {
        id<MTLCommandBuffer> cmdBuffer = [_commandQueue commandBuffer];
        id<MTLRenderCommandEncoder> encoder = [cmdBuffer renderCommandEncoderWithDescriptor:rpd];

        // Aspect ratio correction (letterbox/pillarbox to maintain authentic 3:2 GBA ratio)
        float viewWidth = (float)view.drawableSize.width;
        float viewHeight = (float)view.drawableSize.height;
        float targetAspect = 240.0f / 160.0f;
        float currentAspect = (viewHeight > 0.0f) ? (viewWidth / viewHeight) : targetAspect;
        MTLViewport vp;
        if (currentAspect > targetAspect) {
            float w = viewHeight * targetAspect;
            vp = (MTLViewport){ (viewWidth - w) * 0.5f, 0.0f, w, viewHeight, 0.0f, 1.0f };
        } else {
            float h = (targetAspect > 0.0f) ? (viewWidth / targetAspect) : viewHeight;
            vp = (MTLViewport){ 0.0f, (viewHeight - h) * 0.5f, viewWidth, h, 0.0f, 1.0f };
        }
        [encoder setViewport:vp];

        [encoder setRenderPipelineState:_pipelineState];
        [encoder setFragmentTexture:_texture atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        [encoder endEncoding];

        [cmdBuffer presentDrawable:drawable];
        [cmdBuffer commit];
    }

    // 3. Real-time Metal presentation FPS & In-Game Simulation Speed counter
    _renderedFrames++;
    uint64_t now = mach_absolute_time();
    double elapsed_sec = (double)(now - _lastFpsTime) * _timebase.numer / _timebase.denom / 1000000000.0;
    if (elapsed_sec >= 0.5) {
        double currentFps = (double)_renderedFrames / elapsed_sec;
        double simFps = (double)_simulatedFrames / elapsed_sec;
        double speedPercent = (simFps / 60.0) * 100.0;
        if (speedPercent > 105.0) speedPercent = 100.0; // Paced to 100%

        _renderedFrames = 0;
        _simulatedFrames = 0;
        _lastFpsTime = now;

        static uint64_t lastConsoleLog = 0;
        if (now - lastConsoleLog > (uint64_t)(2.0 * 1000000000.0 * _timebase.denom / _timebase.numer)) {
            printf("[Metal 3 Engine] Presentation: %.1f FPS | Simulation: %.1f GBA FPS (%.0f%% Speed)\n", 
                   currentFps, simFps, speedPercent);
            fflush(stdout);
            lastConsoleLog = now;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            [self->_fpsLabel setStringValue:[NSString stringWithFormat:@"FPS: %.0f (%.0f%% Speed)", currentFps, speedPercent]];
            if (speedPercent >= 85.0) {
                [self->_fpsLabel setTextColor:[NSColor colorWithCalibratedRed:0.2 green:1.0 blue:0.4 alpha:1.0]]; // Green
            } else if (speedPercent >= 60.0) {
                [self->_fpsLabel setTextColor:[NSColor colorWithCalibratedRed:1.0 green:0.85 blue:0.2 alpha:1.0]]; // Yellow
            } else {
                [self->_fpsLabel setTextColor:[NSColor colorWithCalibratedRed:1.0 green:0.3 blue:0.3 alpha:1.0]]; // Red
            }
        });
    }
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    (void)view;
    (void)size;
}

#pragma mark - Keyboard Input

- (void)keyDown:(NSEvent *)event {
    unsigned short code = [event keyCode];

    // Escape (53): Toggle Emulator Pause
    if (code == 53) {
        [self togglePause];
        return;
    }

    switch (code) {
        // D-Pad
        case 126: case 13: keypad_press(&_keypad, GBA_KEY_UP); break;    // Up Arrow / W
        case 125: case 1:  keypad_press(&_keypad, GBA_KEY_DOWN); break;  // Down Arrow / S
        case 123: case 0:  keypad_press(&_keypad, GBA_KEY_LEFT); break;  // Left Arrow / A
        case 124: case 2:  keypad_press(&_keypad, GBA_KEY_RIGHT); break; // Right Arrow / D

        // A Button (Sword / Action / Confirm)
        case 38: case 6: case 36: keypad_press(&_keypad, GBA_KEY_A); break; // J / Z / Enter

        // B Button (Roll / Cancel)
        case 40: case 7: case 49: keypad_press(&_keypad, GBA_KEY_B); break; // K / X / Space

        // Shoulders
        case 12: keypad_press(&_keypad, GBA_KEY_L); break; // Q
        case 14: case 15: keypad_press(&_keypad, GBA_KEY_R); break; // E / R

        // Start / Select
        case 35: keypad_press(&_keypad, GBA_KEY_START); break; // P
        case 17: case 56: case 48: keypad_press(&_keypad, GBA_KEY_SELECT); break; // T / Shift / Tab
        default: break;
    }
}

- (void)keyUp:(NSEvent *)event {
    unsigned short code = [event keyCode];
    if (code == 53) return; // Escape handled on keydown

    switch (code) {
        // D-Pad
        case 126: case 13: keypad_release(&_keypad, GBA_KEY_UP); break;
        case 125: case 1:  keypad_release(&_keypad, GBA_KEY_DOWN); break;
        case 123: case 0:  keypad_release(&_keypad, GBA_KEY_LEFT); break;
        case 124: case 2:  keypad_release(&_keypad, GBA_KEY_RIGHT); break;

        // A Button
        case 38: case 6: case 36: keypad_release(&_keypad, GBA_KEY_A); break;

        // B Button
        case 40: case 7: case 49: keypad_release(&_keypad, GBA_KEY_B); break;

        // Shoulders
        case 12: keypad_release(&_keypad, GBA_KEY_L); break;
        case 14: case 15: keypad_release(&_keypad, GBA_KEY_R); break;

        // Start / Select
        case 35: keypad_release(&_keypad, GBA_KEY_START); break;
        case 17: case 56: case 48: keypad_release(&_keypad, GBA_KEY_SELECT); break;
        default: break;
    }
}

@end

// Application Delegate
@interface GbaAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate> {
    NSWindow* _window;
    GbaMetalView* _metalView;
    NSTimer* _renderTimer;
}
@property (nonatomic, assign) PlatformConfig config;
@end

@implementation GbaAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    NSRect frame = NSMakeRect(0, 0, self.config.window_width, self.config.window_height);

    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:(NSWindowStyleMaskTitled |
                                                     NSWindowStyleMaskClosable |
                                                     NSWindowStyleMaskMiniaturizable |
                                                     NSWindowStyleMaskResizable)
                                            backing:NSBackingStoreBuffered
                                              defer:NO];

    [_window setTitle:[NSString stringWithUTF8String:self.config.window_title]];
    [_window center];
    [_window setDelegate:self];

    _metalView = [[GbaMetalView alloc] initWithFrame:frame
                                            callback:self.config.on_frame
                                            userData:self.config.user_data];
    [_window setContentView:_metalView];
    [_window makeKeyAndOrderFront:nil];
    [_window makeFirstResponder:_metalView];

    // Drive Metal presentation loop at 60 FPS
    _renderTimer = [NSTimer timerWithTimeInterval:1.0 / 60.0
                                           target:self
                                         selector:@selector(onTick:)
                                         userInfo:nil
                                          repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:_renderTimer forMode:NSRunLoopCommonModes];
}

- (void)onTick:(NSTimer *)timer {
    (void)timer;
    [_metalView draw];
}

- (void)windowWillClose:(NSNotification *)notification {
    if (_renderTimer) {
        [_renderTimer invalidate];
        _renderTimer = nil;
    }
    if (_metalView) {
        [_metalView stopEmulation];
    }
    [NSApp terminate:nil];
}

@end

int platform_macos_run(const PlatformConfig* config) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        GbaAppDelegate* delegate = [[GbaAppDelegate alloc] init];
        delegate.config = *config;
        [NSApp setDelegate:delegate];

        [NSApp activateIgnoringOtherApps:YES];
        [NSApp run];
    }
    return 0;
}
