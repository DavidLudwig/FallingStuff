//
//  FSTUFF_Apple.mm
//  FallingStuff
//
//  Created by David Ludwig on 6/4/16.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#include "FSTUFF.h"
#include "FSTUFF_Apple.h"
#include "imgui.h"
#ifdef __OBJC__
#include "imgui_impl_mtl.h"

#include "AAPLShaderTypes.h"

#import <Foundation/Foundation.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#import <GBDeviceInfo/GBDeviceInfo.h>
#elif TARGET_OS_MAC
#import <AppKit/AppKit.h>
#endif

FSTUFF_ViewSize FSTUFF_Apple_GetViewSize(void * _nativeView)
{
    FSTUFF_ViewSize out;

#if TARGET_OS_IOS
    UIScreen * screen = [UIScreen mainScreen]; //self.view.window.screen;
    const float scale = [screen scale];
    float ppi = [GBDeviceInfo deviceInfo].displayInfo.pixelsPerInch;
    if (ppi == 0.f) {
        ppi = 264.f;    // HACK: on iOS simulator (which GBDeviceInfo reports a PPI of zero), use an iPad Air 2's PPI (of 264)
    }
    const float width = ([screen bounds].size.width * scale);
    const float height = ([screen bounds].size.height * scale);
    out.widthMM = (width / ppi) * 25.4f;
    out.heightMM = (height / ppi) * 25.4f;
    out.widthPixels = std::lround(width);
    out.heightPixels = std::lround(height);
    out.widthOS = std::lround([screen bounds].size.width);
    out.heightOS = std::lround([screen bounds].size.height);
#else
    FSTUFF_Log("%s, _nativeView: %p\n", __FUNCTION__, _nativeView);
    NSView * nativeView = (__bridge NSView *) _nativeView;
    FSTUFF_Log(@"%s, nativeView: %@\n", __FUNCTION__, nativeView);
    FSTUFF_Log(@"%s, nativeView.window: %@\n", __FUNCTION__, nativeView.window);
    FSTUFF_Log("%s, nativeView.frame: {%f,%f,%f,%f}\n",
        __FUNCTION__,
        nativeView.frame.origin.x,
        nativeView.frame.origin.y,
        nativeView.frame.size.width,
        nativeView.frame.size.height
    );
    NSScreen * screen = nativeView.window.screen;
    FSTUFF_Log(@"%s, screen: %@\n", __FUNCTION__, screen);
    NSNumber * displayID = [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
    FSTUFF_Log(@"%s, displayID: %@\n", __FUNCTION__, displayID);
    CGDirectDisplayID cgDisplayID = (CGDirectDisplayID) [displayID intValue];
    FSTUFF_Log("%s, cgDisplayID: %d\n", __FUNCTION__, cgDisplayID);
    const CGSize scrSizeMM = CGDisplayScreenSize(cgDisplayID);
    FSTUFF_Log("%s, scrSizeMM: {%f,%f}\n", __FUNCTION__, scrSizeMM.width, scrSizeMM.height);

    const CGSize ptsToMM = CGSizeMake(scrSizeMM.width / screen.frame.size.width,
                                      scrSizeMM.height / screen.frame.size.height);
    out.widthMM = nativeView.bounds.size.width * ptsToMM.width;
    out.heightMM = nativeView.bounds.size.height * ptsToMM.height;

    if ([nativeView isKindOfClass:[MTKView class]]) {
        MTKView * mtkView = (MTKView *) nativeView;
        FSTUFF_Log("%s, drawableSize: {%f,%f}\n", __FUNCTION__, mtkView.drawableSize.width, mtkView.drawableSize.height);
        out.widthPixels = (int) std::lround(mtkView.drawableSize.width);
        out.heightPixels = (int) std::lround(mtkView.drawableSize.height);
    } else {
        out.widthPixels = (int) std::lround(nativeView.bounds.size.width);
        out.heightPixels = (int) std::lround(nativeView.bounds.size.height);
    }
    out.widthOS = (int) std::lround(nativeView.bounds.size.width);
    out.heightOS = (int) std::lround(nativeView.bounds.size.height);

#endif

//    FSTUFF_Log(@"view size: {%f,%f} (pts?) --> {%f,%f} (mm?)\n",
//          self.view.bounds.size.width,
//          self.view.bounds.size.height,
//          viewSizeMM.width,
//          viewSizeMM.height);

    FSTUFF_Log(@"%s, view size: {%f,%f} (mm?)\n",
        __FUNCTION__,
        out.widthMM,
        out.heightMM);
    return out;
}

void FSTUFF_Log(NSString * fmt, va_list args) {
    static FILE * outFile = NULL;
    static std::once_flag once;
    bool first = false;

    std::call_once(once, [&] {
        NSString * logFilePath = [NSString pathWithComponents:@[
            NSHomeDirectory(),
            @"Library",
            @"Logs",
            @"FallingStuff.log"
        ]];
        outFile = fopen([logFilePath UTF8String], "w");
        first = true;
    });
    if (first) {
        FSTUFF_Log("----------------------------------------------------------------------\nFalling Stuff, startup, Build-Date:\"%s\", Build-Number:%d\n", __DATE__, FSTUFF_BuildNumber);
    }
    if (fmt) {
        NSString * buffer = [[NSString alloc] initWithFormat:fmt arguments:args];
        if (outFile) {
            fprintf(outFile, "%s", [buffer UTF8String]);
            fflush(outFile);
        }
        if (outFile != stdout && outFile != stderr) {
            fprintf(stdout, "%s", [buffer UTF8String]);
        }
    }
}

void FSTUFF_Log(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    NSString * nsfmt = [[NSString alloc] initWithUTF8String:fmt];
    FSTUFF_Log(nsfmt, args);
    va_end(args);
}


#import "FSTUFF_AppleMetalStructs.h"

#if TARGET_OS_IOS
#import <GBDeviceInfo/GBDeviceInfo.h>
#endif


@interface FSTUFF_AppleMetalView : MTKView
@end

@implementation FSTUFF_AppleMetalView
- (BOOL) acceptsFirstResponder {
    return YES;
}
@end


#pragma mark - Renderer

void FSTUFF_Apple_CopyMatrix(matrix_float4x4 & dest, const gbMat4 & src)
{
    memcpy((void *)&(dest), (const void *)&(src.e), sizeof(float) * 16);
}

void FSTUFF_Apple_CopyMatrix(gbMat4 & dest, const matrix_float4x4 & src)
{
    memcpy((void *)&(dest.e), (const void *)&(src), sizeof(float) * 16);
}

void FSTUFF_Apple_CopyVector(vector_float4 & dest, const gbVec4 & src)
{
    memcpy((void *)&(dest), (const void *)&(src.e), sizeof(float) * 4);
}

void FSTUFF_Apple_CopyVector(gbMat4 & dest, const vector_float4 & src)
{
    memcpy((void *)&(dest.e), (const void *)&(src), sizeof(float) * 4);
}

void FSTUFF_AppleMetalRenderer::DestroyVertexBuffer(void * _gpuVertexBuffer)
{
    id <MTLBuffer> gpuVertexBuffer = (__bridge_transfer id <MTLBuffer>)_gpuVertexBuffer;
    gpuVertexBuffer = nil;
}

void * FSTUFF_AppleMetalRenderer::NewVertexBuffer(void * src, size_t size)
{
    return (__bridge_retained void *)[this->device newBufferWithBytes:src length:size options:MTLResourceOptionCPUCacheModeDefault];
}

void FSTUFF_AppleMetalRenderer::ViewChanged()
{
    const FSTUFF_ViewSize viewSize = this->GetViewSize();

    MTLTextureDescriptor *simTextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                                     width:viewSize.widthPixels
                                                                                                    height:viewSize.heightPixels
                                                                                                 mipmapped:NO];
    simTextureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    this->simTexture = [this->device newTextureWithDescriptor:simTextureDescriptor];
    this->simTexture.label = @"FSTUFF Simulation Texture";
    FSTUFF_Log(@"%s, renderer->simTexture = %@\n", __FUNCTION__, this->simTexture);
}

FSTUFF_ViewSize FSTUFF_AppleMetalRenderer::GetViewSize()
{
    return FSTUFF_Apple_GetViewSize((__bridge void *)(this->nativeView));
}

void FSTUFF_AppleMetalRenderer::RenderShapes(FSTUFF_Shape * shape, size_t offset, size_t count, float alpha)
{
    // Metal can raise a program-crashing assertion, if zero amount of shapes attempts to get
    // rendered.
    if (count == 0) {
        return;
    }

    id <MTLDevice> gpuDevice = this->device;
    id <MTLRenderCommandEncoder> renderCommandEncoder = this->simRenderCommandEncoder;
    id <MTLBuffer> gpuData = (__bridge id <MTLBuffer>) this->appData;

    MTLPrimitiveType gpuPrimitiveType;
    switch (shape->primitiveType) {
        case FSTUFF_PrimitiveLineStrip:
            gpuPrimitiveType = MTLPrimitiveTypeLineStrip;
            break;
        case FSTUFF_PrimitiveTriangles:
            gpuPrimitiveType = MTLPrimitiveTypeTriangle;
            break;
        case FSTUFF_PrimitiveTriangleFan:
            gpuPrimitiveType = MTLPrimitiveTypeTriangleStrip;
            break;
        default:
            FSTUFF_Log(@"Unknown or unmapped FSTUFF_PrimitiveType in shape: %u\n", shape->primitiveType);
            return;
    }

    NSUInteger shapesOffsetInGpuData;
    switch (shape->type) {
        case FSTUFF_ShapeCircle:
            shapesOffsetInGpuData = offsetof(FSTUFF_GPUData, circles);
            break;
        case FSTUFF_ShapeBox:
            shapesOffsetInGpuData = offsetof(FSTUFF_GPUData, boxes);
            break;
        default:
            FSTUFF_Log(@"Unknown or unmapped FSTUFF_ShapeType in shape: %u\n", shape->type);
            return;
    }

    [renderCommandEncoder pushDebugGroup:[[NSString alloc] initWithUTF8String:shape->debugName]];
    [renderCommandEncoder setVertexBuffer:(__bridge id <MTLBuffer>)shape->gpuVertexBuffer offset:0 atIndex:0];   // 'position[<vertex id>]'
    [renderCommandEncoder setVertexBuffer:gpuData offset:offsetof(FSTUFF_GPUData, globals) atIndex:1];           // 'gpuGlobals'
    [renderCommandEncoder setVertexBuffer:gpuData offset:shapesOffsetInGpuData atIndex:2];                       // 'gpuShapes[<instance id>]'
    [renderCommandEncoder setVertexBytes:&alpha length:sizeof(alpha) atIndex:3];                                 // 'alpha'

#if TARGET_OS_IOS
    const MTLFeatureSet featureSetForBaseInstance = MTLFeatureSet_iOS_GPUFamily3_v1;
#else
    const MTLFeatureSet featureSetForBaseInstance = MTLFeatureSet_OSX_GPUFamily1_v1;
#endif
    if (offset == 0) {
        [renderCommandEncoder drawPrimitives:gpuPrimitiveType
                        vertexStart:0
                        vertexCount:shape->numVertices
                      instanceCount:count];
    } else if ([gpuDevice supportsFeatureSet:featureSetForBaseInstance]) {
        [renderCommandEncoder drawPrimitives:gpuPrimitiveType
                                 vertexStart:0
                                 vertexCount:shape->numVertices
                               instanceCount:count
                                baseInstance:offset];
    }
    [renderCommandEncoder popDebugGroup];

}

void FSTUFF_AppleMetalRenderer::SetProjectionMatrix(const gbMat4 & matrix)
{
    FSTUFF_Apple_CopyMatrix(this->appData->globals.projection_matrix, matrix);
}

void FSTUFF_AppleMetalRenderer::SetShapeProperties(FSTUFF_ShapeType shape, size_t i, const gbMat4 & matrix, const gbVec4 & color)
{
    switch (shape) {
        case FSTUFF_ShapeCircle: {
            FSTUFF_Apple_CopyMatrix(this->appData->circles[i].model_matrix, matrix);
            FSTUFF_Apple_CopyVector(this->appData->circles[i].color, color);
        } break;
        case FSTUFF_ShapeBox: {
            FSTUFF_Apple_CopyMatrix(this->appData->boxes[i].model_matrix, matrix);
            FSTUFF_Apple_CopyVector(this->appData->boxes[i].color, color);
        } break;
    }
}

//- (NSPoint)mouseLocationFromEvent:(NSEvent *)nsEvent
//{
//    const NSPoint posInWindow = [nsEvent locationInWindow];
//    const NSPoint posInView = [renderer->nativeView convertPoint:posInWindow fromView:nil];
//
//    // Cocoa views seem to like making Y=0 be at the bottom of the view, rather than at the top.
//    // ImGui wants coordinates with Y=0 being at the top, so, convert to that!
//    const CGFloat viewHeight = renderer->nativeView.bounds.size.height;
//    const NSPoint posWithYFlip = {posInView.x, viewHeight - posInView.y};
//    return posWithYFlip;
//}

FSTUFF_CursorInfo FSTUFF_AppleMetalRenderer::GetCursorInfo()
{
    const NSUInteger pressedMouseButtons = [NSEvent pressedMouseButtons];
    const NSPoint mouseLocation = [NSEvent mouseLocation];
    const NSPoint windowPos = this->nativeView.window.frame.origin;
    const NSPoint posInWindow = {
        mouseLocation.x - windowPos.x,
        mouseLocation.y - windowPos.y
    };
    const NSPoint posInView = [this->nativeView convertPoint:posInWindow fromView:nil];
    
    // Cocoa views seem to like making Y=0 be at the bottom of the view, rather than at the top.
    // ImGui wants coordinates with Y=0 being at the top, so, convert to that!
    const CGFloat viewHeight = this->nativeView.bounds.size.height;
    const NSPoint posWithYFlip = {posInView.x, viewHeight - posInView.y};
    
    FSTUFF_CursorInfo out;
    out.xOS = posWithYFlip.x;
    out.yOS = posWithYFlip.y;
//    FSTUFF_Log("** GetPos: pos={%.0f,%.0f}, btns=%lu\n", posWithYFlip.x, posWithYFlip.y, (unsigned long)pressedMouseButtons);
//    FSTUFF_Log("pressedMouseButtons: %lu\n", (unsigned long)pressedMouseButtons);
    out.pressed = (pressedMouseButtons != 0);
//    out.contained = NSPointInRect(this->nativeView.window.)
    return out;
}



@interface FSTUFF_AppleMetalViewController()
@property (nonatomic, strong) MTKView *theView;
@end

@implementation FSTUFF_AppleMetalViewController
{
    // renderer
    FSTUFF_AppleMetalRenderer * renderer;

    // game
    FSTUFF_Simulation * sim;
    
    // Cursor tracking area
    NSTrackingArea * area;
}

- (FSTUFF_Simulation *) sim
{
    @synchronized(self) {
        if ( ! sim) {
            sim = new FSTUFF_Simulation();
        }
        return sim;
    }
}

- (void)dealloc
{
    if (sim) {
        delete sim;
        sim = NULL;
    }
    
    if (renderer) {
        delete renderer;
        renderer = NULL;
    }
    
    ImGui_ImplMtl_Shutdown();
}

- (void)loadView
{
    self.view = [[FSTUFF_AppleMetalView alloc] init];
}

- (void)updateTrackingArea
{
    if (area) {
        [self.view removeTrackingArea:area];
        area = nil;
    }

    area = [[NSTrackingArea alloc] initWithRect:[self.view bounds]
                                        options:(NSTrackingActiveAlways | NSTrackingInVisibleRect |
                         NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved)
                                          owner:self
                                       userInfo:nil];
    [self.view addTrackingArea:area];
}

- (void)viewDidLoad
{
//    FSTUFF_Log("FSTUFF_AppleMetalViewController viewDidLoad, starting: frame:{%f,%f,%f,%f}\n",
//        self.view.frame.origin.x,
//        self.view.frame.origin.y,
//        self.view.frame.size.width,
//        self.view.frame.size.height
//    );
    [super viewDidLoad];
    
    //_sim = new FSTUFF_Simulation();
    renderer = new FSTUFF_AppleMetalRenderer();

    renderer->constantDataBufferIndex = 0;
    renderer->_inflight_semaphore = dispatch_semaphore_create(3);

    // Set the view to use the default, Metal device
    renderer->device = MTLCreateSystemDefaultDevice();

    // Create a new, Metal command queue
    renderer->commandQueue = [renderer->device newCommandQueue];
    
    // Load all the shader files with a metal file extension in the project
    //
    // Get the path to the bundle, in a manner that works with macOS's ScreenSaverEngine.app.
    // This app stores screensavers as separate bundles.  We need to find the bundle for the
    // screensaver that we are using.
    NSBundle * bundle = [NSBundle bundleForClass:[self class]];
    NSString * path = [bundle pathForResource:@"default" ofType:@"metallib"];
    NSError * err = nil;
    renderer->defaultLibrary = [renderer->device newLibraryWithFile:path error:&err];
    if ( ! renderer->defaultLibrary) {
        FSTUFF_Log(@"Failed to create Metal library, error:%@\n", err);
    }

    if (renderer->device) {
        // Setup view
        renderer->nativeView = (MTKView *)self.view;
        renderer->nativeView.delegate = self;
        renderer->nativeView.device = renderer->device;
        self.sim->renderer = renderer;

        // Setup a texture to draw the simulation to
        renderer->ViewChanged();

        // Describe stuff common to all pipeline states
        MTLRenderPipelineDescriptor *pipelineStateDescriptor = nil;

        pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineStateDescriptor.label = @"FSTUFF_SimulationPipeline";
        pipelineStateDescriptor.sampleCount = renderer->nativeView.sampleCount;
        pipelineStateDescriptor.fragmentFunction = [renderer->defaultLibrary newFunctionWithName:@"FSTUFF_FragmentShader"];
        pipelineStateDescriptor.vertexFunction = [renderer->defaultLibrary newFunctionWithName:@"FSTUFF_VertexShader"];
//        pipelineStateDescriptor.colorAttachments[0].pixelFormat = _renderer->nativeView.colorPixelFormat;
        pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        pipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
        pipelineStateDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        pipelineStateDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        renderer->simulationPipelineState = [renderer->device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&err];
        if ( ! renderer->simulationPipelineState) {
            FSTUFF_Log(@"Failed to create pipeline state, error:%@\n", err);
        }


//        pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
//        pipelineStateDescriptor.label = @"FSTUFF_MainPipeline";
//        pipelineStateDescriptor.sampleCount = _renderer->nativeView.sampleCount;
//        pipelineStateDescriptor.fragmentFunction = [_renderer->defaultLibrary newFunctionWithName:@"samplingShader"];
////        pipelineStateDescriptor.fragmentFunction = [_renderer->defaultLibrary newFunctionWithName:@"FSTUFF_FragmentShader"];
//        pipelineStateDescriptor.vertexFunction = [_renderer->defaultLibrary newFunctionWithName:@"vertexShader"];
//        pipelineStateDescriptor.colorAttachments[0].pixelFormat = _renderer->nativeView.colorPixelFormat;
//        pipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
//        pipelineStateDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
//        pipelineStateDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
//        pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
//        pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
//        pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
//        pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineStateDescriptor.label = @"FSTUFF_MainPipeline";
        pipelineStateDescriptor.vertexFunction = [renderer->defaultLibrary newFunctionWithName:@"vertexShader"];;
        pipelineStateDescriptor.fragmentFunction = [renderer->defaultLibrary newFunctionWithName:@"samplingShader"];
        pipelineStateDescriptor.colorAttachments[0].pixelFormat = renderer->nativeView.colorPixelFormat;

        renderer->mainPipelineState = [renderer->device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&err];
        if ( ! renderer->mainPipelineState) {
            FSTUFF_Log(@"Failed to create overlay pipeline state, error:%@\n", err);
        }


        // Create a single, unchanging vertex buffer, for use in blending layers
        // (such as for the simulation, and any overlays).
        static const AAPLVertex quadVertices[] =
        {
            { {  1,   1 },  { 1.f, 0.f } },
            { { -1,   1 },  { 0.f, 0.f } },
            { { -1,  -1 },  { 0.f, 1.f } },

            { {  1,   1 },  { 1.f, 0.f } },
            { { -1,  -1 },  { 0.f, 1.f } },
            { {  1,  -1 },  { 1.f, 1.f } },
        };

        // Create our vertex buffer, and initialize it with our quadVertices array
        renderer->_vertices = [renderer->device newBufferWithBytes:quadVertices
                                         length:sizeof(quadVertices)
                                        options:MTLResourceStorageModeShared];

        // Calculate the number of vertices by dividing the byte length by the size of each vertex
        renderer->_numVertices = sizeof(quadVertices) / sizeof(AAPLVertex);


        // allocate a number of buffers in memory that matches the sempahore count so that
        // we always have one self contained memory buffer for each buffered frame.
        // In this case triple buffering is the optimal way to go so we cycle through 3 memory buffers
        for (int i = 0; i < FSTUFF_MaxInflightBuffers; i++) {
            renderer->gpuConstants[i] = [renderer->device newBufferWithLength:FSTUFF_MaxBytesPerFrame options:0];
            renderer->gpuConstants[i].label = [NSString stringWithFormat:@"FSTUFF_ConstantBuffer%i", i];
        }
        
        ImGui_ImplMtl_Init(self.sim, true);

        
    } else { // Fallback to a blank NSView, an application could also fallback to OpenGL here.
        FSTUFF_Log(@"Metal is not supported on this device\n");
#if ! TARGET_OS_IOS
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
#endif
    }


    [self updateTrackingArea];
    self.sim->UpdateCursorInfo(renderer->GetCursorInfo());
}

#if ! TARGET_OS_IOS
- (void)keyDown:(NSEvent *)theEvent
{
    FSTUFF_Event event = FSTUFF_Event::NewKeyEvent(FSTUFF_EventKeyDown, [theEvent.characters cStringUsingEncoding:NSUTF8StringEncoding]);
    self.sim->EventReceived(&event);
    if ( ! event.handled) {
        [super keyDown:theEvent];
    }
}

- (void)keyUp:(NSEvent *)theEvent
{
    FSTUFF_Event event = FSTUFF_Event::NewKeyEvent(FSTUFF_EventKeyUp, [theEvent.characters cStringUsingEncoding:NSUTF8StringEncoding]);
    self.sim->EventReceived(&event);
    if ( ! event.handled) {
        [super keyUp:theEvent];
    }
}


- (NSPoint)mouseLocationFromEvent:(NSEvent *)nsEvent
{
    const NSPoint posInWindow = [nsEvent locationInWindow];
    const NSPoint posInView = [renderer->nativeView convertPoint:posInWindow fromView:nil];
    
    // Cocoa views seem to like making Y=0 be at the bottom of the view, rather than at the top.
    // ImGui wants coordinates with Y=0 being at the top, so, convert to that!
    const CGFloat viewHeight = renderer->nativeView.bounds.size.height;
    const NSPoint posWithYFlip = {posInView.x, viewHeight - posInView.y};
    return posWithYFlip;
}

- (void)mouseDown:(NSEvent *)nsEvent
{
//    const NSPoint pos = [self mouseLocationFromEvent:nsEvent];
//    const FSTUFF_CursorInfo cur = renderer->GetCursorInfo();
//    FSTUFF_Log("mouse down: event={%f,%f}, get={%f,%f}\n", pos.x, pos.y, cur.xOS, cur.yOS);
//    FSTUFF_Event event = FSTUFF_Event::NewCursorButtonEvent(pos.x, pos.y, true);
//    self.sim->EventReceived(&event);
//    if ( ! event.handled) {
//        [super mouseDown:nsEvent];
//    }

    sim->UpdateCursorInfo(renderer->GetCursorInfo());
    [super mouseDown:nsEvent];
}

- (void)mouseUp:(NSEvent *)nsEvent
{
//    const NSPoint pos = [self mouseLocationFromEvent:nsEvent];
////    FSTUFF_Log("mouse up: {%f, %f}\n", pos.x, pos.y);
//    FSTUFF_Event event = FSTUFF_Event::NewCursorButtonEvent(pos.x, pos.y, false);
//    self.sim->EventReceived(&event);
//    if ( ! event.handled) {
//        [super mouseUp:nsEvent];
//    }

    sim->UpdateCursorInfo(renderer->GetCursorInfo());
    [super mouseUp:nsEvent];
}

- (void)mouseMoved:(NSEvent *)nsEvent
{
//    const NSPoint pos = [self mouseLocationFromEvent:nsEvent];
////    FSTUFF_Log("mouse moved: {%f, %f}\n", pos.x, pos.y);
//    FSTUFF_Event event = FSTUFF_Event::NewCursorMotionEvent(pos.x, pos.y);
//    self.sim->EventReceived(&event);
//    if ( ! event.handled) {
//        [super mouseMoved:nsEvent];
//    }

    sim->UpdateCursorInfo(renderer->GetCursorInfo());
    [super mouseMoved:nsEvent];
}

- (void)mouseDragged:(NSEvent *)nsEvent
{
//    const NSPoint pos = [self mouseLocationFromEvent:nsEvent];
////    FSTUFF_Log("mouse dragged: {%f, %f}\n", pos.x, pos.y);
//    FSTUFF_Event event = FSTUFF_Event::NewCursorMotionEvent(pos.x, pos.y);
//    self.sim->EventReceived(&event);
//    if ( ! event.handled) {
//        [super mouseDragged:nsEvent];
//    }

    sim->UpdateCursorInfo(renderer->GetCursorInfo());
    [super mouseDragged:nsEvent];
}

- (void)mouseEntered:(NSEvent *)nsEvent
{
//    FSTUFF_Event event = FSTUFF_Event::NewCursorContainedEvent(true);
//    self.sim->EventReceived(&event);
//    if ( ! event.handled) {
//        [super mouseEntered:nsEvent];
//    }

//    sim->UpdateCursorInfo(renderer->GetCursorInfo());
//    [super mouseEntered:nsEvent];
}

- (void)mouseExited:(NSEvent *)nsEvent
{
//    FSTUFF_Event event = FSTUFF_Event::NewCursorContainedEvent(false);
//    self.sim->EventReceived(&event);
//    if ( ! event.handled) {
//        [super mouseExited:nsEvent];
//    }

//    sim->UpdateCursorInfo(renderer->GetCursorInfo());
//    [super mouseExited:nsEvent];
}


#endif

//#ifndef _MTLFeatureSet_iOS_GPUFamily3_v1

// Called whenever view changes orientation or layout is changed
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    renderer->nativeView = (MTKView *) self.view;
    renderer->ViewChanged();
    [self updateTrackingArea];
    const FSTUFF_ViewSize viewSize = renderer->GetViewSize();
    self.sim->ViewChanged(viewSize);
}

// Called whenever the view needs to render
- (void)drawInMTKView:(nonnull MTKView *)view
{
    @autoreleasepool {
        dispatch_semaphore_wait(renderer->_inflight_semaphore, DISPATCH_TIME_FOREVER);

        // Tell ImGUI that a new frame is starting.
        ImGui_ImplMtl_NewFrame(self.sim->viewSize);

        // Update FSTUFF state
        renderer->appData = (FSTUFF_GPUData *) [renderer->gpuConstants[renderer->constantDataBufferIndex] contents];
        self.sim->Update();

        // Start drawing previously-requested ImGUI content, to an
        // offscreen texture.
        ImGui::Render();    // renders to a texture that's retrieve-able via ImGui_ImplMtl_MainTexture()
        
        // Finish-up the rendering of ImGUI content.
        ImGui_ImplMtl_EndFrame();

        // Create a new command buffer for each renderpass to the current drawable
        id <MTLCommandBuffer> commandBuffer = [renderer->commandQueue commandBuffer];
        commandBuffer.label = @"FSTUFF_CommandBuffer";

        // Call the view's completion handler which is required by the view since it will signal its semaphore and set up the next buffer
        __block dispatch_semaphore_t block_sema = renderer->_inflight_semaphore;
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
            dispatch_semaphore_signal(block_sema);
        }];
        
        // Obtain a renderPassDescriptor generated from the view's drawable textures
        MTLRenderPassDescriptor* mainRenderPass = renderer->nativeView.currentRenderPassDescriptor;

        if (mainRenderPass) { // If we have a valid drawable, begin the commands to render into it

            // Create a render pass, for the simulation
            MTLRenderPassDescriptor *simRenderPass = [MTLRenderPassDescriptor renderPassDescriptor];
        //    renderPassDescriptor.colorAttachments[0].texture = [(id<CAMetalDrawable>)g_MtlCurrentDrawable texture];
            simRenderPass.colorAttachments[0].texture = renderer->simTexture;
            simRenderPass.colorAttachments[0].loadAction = MTLLoadActionClear;
            simRenderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
            simRenderPass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);

            // Create render command encoders so we can render into something
            id <MTLRenderCommandEncoder> simRenderCommandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:simRenderPass];
            renderer->simRenderCommandEncoder = simRenderCommandEncoder;
            simRenderCommandEncoder.label = @"FSTUFF_SimRenderEncoder";
            [simRenderCommandEncoder setRenderPipelineState:renderer->simulationPipelineState];

            // Draw shapes
            renderer->appData = (__bridge FSTUFF_GPUData *)renderer->gpuConstants[renderer->constantDataBufferIndex];
            self.sim->Render();
            
            // We're done encoding commands
            [simRenderCommandEncoder endEncoding];
            renderer->simRenderCommandEncoder = nil;

            // Create another render command encoder so we can combine the layers
            id <MTLRenderCommandEncoder> mainRenderCommandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:mainRenderPass];
            renderer->mainRenderCommandEncoder = mainRenderCommandEncoder;
            mainRenderCommandEncoder.label = @"FSTUFF_MainRenderEncoder";
            [mainRenderCommandEncoder setRenderPipelineState:renderer->mainPipelineState];
            [mainRenderCommandEncoder setVertexBuffer:renderer->_vertices
                                               offset:0
                                              atIndex:AAPLVertexInputIndexVertices];
            [mainRenderCommandEncoder setFragmentTexture:renderer->simTexture
                                                 atIndex:AAPLTextureIndexBaseColor];
            [mainRenderCommandEncoder setFragmentTexture:ImGui_ImplMtl_MainTexture()
                                                 atIndex:AAPLTextureIndexOverlayColor];
            [mainRenderCommandEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                                         vertexStart:0
                                         vertexCount:renderer->_numVertices];
            [mainRenderCommandEncoder endEncoding];
            renderer->mainRenderCommandEncoder = nil;

            // Schedule a present once the framebuffer is complete using the current drawable
            [commandBuffer presentDrawable:renderer->nativeView.currentDrawable];
        }

        // The render assumes it can now increment the buffer index and that the previous index won't be touched until we cycle back around to the same index
        renderer->constantDataBufferIndex = (renderer->constantDataBufferIndex + 1) % FSTUFF_MaxInflightBuffers;

        // Finalize rendering here & push the command buffer to the GPU
        [commandBuffer commit];
    }
}

@end



void FSTUFF_Log(NSString * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    FSTUFF_Log(fmt, args);
    va_end(args);
}

#endif

