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
#include "imgui_impl_mtl.h"
#ifdef __OBJC__

#import <Foundation/Foundation.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#import <GBDeviceInfo/GBDeviceInfo.h>
#elif TARGET_OS_MAC
#import <AppKit/AppKit.h>
#endif

void FSTUFF_Apple_GetViewSizeMM(void * _nativeView, float * outWidthMM, float * outHeightMM)
{
#if TARGET_OS_IOS
    UIScreen * screen = [UIScreen mainScreen]; //self.view.window.screen;
    const float scale = [screen scale];
    float ppi = [GBDeviceInfo deviceInfo].displayInfo.pixelsPerInch;
    if (ppi == 0.f) {
        ppi = 264.f;    // HACK: on iOS simulator (which GBDeviceInfo reports a PPI of zero), use an iPad Air 2's PPI (of 264)
    }
    const float width = ([screen bounds].size.width * scale);
    const float height = ([screen bounds].size.height * scale);
    *outWidthMM = (width / ppi) * 25.4f;
    *outHeightMM = (height / ppi) * 25.4f;
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
    *outWidthMM = nativeView.bounds.size.width * ptsToMM.width;
    *outHeightMM = nativeView.bounds.size.height * ptsToMM.height;
#endif

//    FSTUFF_Log(@"view size: {%f,%f} (pts?) --> {%f,%f} (mm?)\n",
//          self.view.bounds.size.width,
//          self.view.bounds.size.height,
//          viewSizeMM.width,
//          viewSizeMM.height);

    FSTUFF_Log(@"%s, view size: {%f,%f} (mm?)\n",
        __FUNCTION__,
        *outWidthMM,
        *outHeightMM);
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

void FSTUFF_AppleMetalRenderer::GetViewSizeMM(float *outWidthMM, float *outHeightMM)
{
    return FSTUFF_Apple_GetViewSizeMM((__bridge void *)(this->nativeView), outWidthMM, outHeightMM);
}

void FSTUFF_AppleMetalRenderer::RenderShapes(FSTUFF_Shape * shape, size_t offset, size_t count, float alpha)
{
    // Metal can raise a program-crashing assertion, if zero amount of shapes attempts to get
    // rendered.
    if (count == 0) {
        return;
    }

    id <MTLDevice> gpuDevice = this->device;
    id <MTLRenderCommandEncoder> renderCommandEncoder = this->renderCommandEncoder;
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


@interface FSTUFF_AppleMetalViewController()
@property (nonatomic, strong) MTKView *theView;
@end

@implementation FSTUFF_AppleMetalViewController
{
    // renderer
    FSTUFF_AppleMetalRenderer * _renderer;

    // game
    FSTUFF_Simulation * _sim;
}

- (FSTUFF_Simulation *) sim
{
    @synchronized(self) {
        if ( ! _sim) {
            _sim = new FSTUFF_Simulation();
        }
        return _sim;
    }
}

- (void)dealloc
{
    if (_sim) {
        delete _sim;
        _sim = NULL;
    }
    
    if (_renderer) {
        delete _renderer;
        _renderer = NULL;
    }
    
    ImGui_ImplMtl_Shutdown();
}

- (void)loadView
{
    self.view = [[FSTUFF_AppleMetalView alloc] init];
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
    _renderer = new FSTUFF_AppleMetalRenderer();

    _renderer->constantDataBufferIndex = 0;
    _renderer->_inflight_semaphore = dispatch_semaphore_create(3);

    // Set the view to use the default, Metal device
    _renderer->device = MTLCreateSystemDefaultDevice();

    // Create a new, Metal command queue
    _renderer->commandQueue = [_renderer->device newCommandQueue];
    
    // Load all the shader files with a metal file extension in the project
    //
    // Get the path to the bundle, in a manner that works with macOS's ScreenSaverEngine.app.
    // This app stores screensavers as separate bundles.  We need to find the bundle for the
    // screensaver that we are using.
    NSBundle * bundle = [NSBundle bundleForClass:[self class]];
    NSString * path = [bundle pathForResource:@"default" ofType:@"metallib"];
    NSError * err = nil;
    _renderer->defaultLibrary = [_renderer->device newLibraryWithFile:path error:&err];
    if ( ! _renderer->defaultLibrary) {
        FSTUFF_Log(@"Failed to create Metal library, error:%@\n", err);
    }

    if (_renderer->device) {
        // Setup view
        _renderer->nativeView = (MTKView *)self.view;
        _renderer->nativeView.delegate = self;
        _renderer->nativeView.device = _renderer->device;
        self.sim->renderer = _renderer;

        // Describe stuff common to all pipeline states
        MTLRenderPipelineDescriptor *pipelineStateDescriptor = nil;
        pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineStateDescriptor.label = @"FSTUFF_Pipeline";
        pipelineStateDescriptor.sampleCount = _renderer->nativeView.sampleCount;
        pipelineStateDescriptor.fragmentFunction = [_renderer->defaultLibrary newFunctionWithName:@"FSTUFF_FragmentShader"];
        pipelineStateDescriptor.vertexFunction = [_renderer->defaultLibrary newFunctionWithName:@"FSTUFF_VertexShader"];
        pipelineStateDescriptor.colorAttachments[0].pixelFormat = _renderer->nativeView.colorPixelFormat;
        pipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
        pipelineStateDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        pipelineStateDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        _renderer->pipelineState = [_renderer->device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&err];
        if ( ! _renderer->pipelineState) {
            FSTUFF_Log(@"Failed to create pipeline state, error:%@\n", err);
        }

#if 0
        pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineStateDescriptor.label = @"FSTUFF_OverlayPipeline";
        pipelineStateDescriptor.sampleCount = _renderer->nativeView.sampleCount;
        pipelineStateDescriptor.fragmentFunction = [_renderer->defaultLibrary newFunctionWithName:@"FSTUFF_FragmentShader"];
        pipelineStateDescriptor.vertexFunction = [_renderer->defaultLibrary newFunctionWithName:@"FSTUFF_VertexShader"];
        pipelineStateDescriptor.colorAttachments[0].pixelFormat = _renderer->nativeView.colorPixelFormat;
        pipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
        pipelineStateDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        pipelineStateDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        _renderer->pipelineState = [_renderer->device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&err];
        if ( ! _renderer->pipelineState) {
            FSTUFF_Log(@"Failed to create overlay pipeline state, error:%@\n", err);
        }
#endif


//        _renderer->nativeView = _view;

        // allocate a number of buffers in memory that matches the sempahore count so that
        // we always have one self contained memory buffer for each buffered frame.
        // In this case triple buffering is the optimal way to go so we cycle through 3 memory buffers
        for (int i = 0; i < FSTUFF_MaxInflightBuffers; i++) {
            _renderer->gpuConstants[i] = [_renderer->device newBufferWithLength:FSTUFF_MaxBytesPerFrame options:0];
            _renderer->gpuConstants[i].label = [NSString stringWithFormat:@"FSTUFF_ConstantBuffer%i", i];
        }
        
        ImGui_ImplMtl_Init(self.sim, true);

        
    } else { // Fallback to a blank NSView, an application could also fallback to OpenGL here.
        FSTUFF_Log(@"Metal is not supported on this device\n");
#if ! TARGET_OS_IOS
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
#endif
    }
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
#endif

//#ifndef _MTLFeatureSet_iOS_GPUFamily3_v1

// Called whenever view changes orientation or layout is changed
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    float widthMM, heightMM;
    _renderer->nativeView = (MTKView *) self.view;
    _renderer->GetViewSizeMM(&widthMM, &heightMM);
    self.sim->ViewChanged(widthMM, heightMM);
}

// Called whenever the view needs to render
- (void)drawInMTKView:(nonnull MTKView *)view
{
    @autoreleasepool {
        dispatch_semaphore_wait(_renderer->_inflight_semaphore, DISPATCH_TIME_FOREVER);
        
        // Update ImGUI state
        ImGui_ImplMtl_NewFrame();
        ImGui::ShowDemoWindow();
        ImGui::Render();    // renders to a texture that's retrieve-able via ImGui_ImplMtl_MainTexture()
        
        // Update FSTUFF state
        _renderer->appData = (FSTUFF_GPUData *) [_renderer->gpuConstants[_renderer->constantDataBufferIndex] contents];
        self.sim->Update();

        // Create a new command buffer for each renderpass to the current drawable
        id <MTLCommandBuffer> commandBuffer = [_renderer->commandQueue commandBuffer];
        commandBuffer.label = @"FSTUFF_CommandBuffer";

        // Call the view's completion handler which is required by the view since it will signal its semaphore and set up the next buffer
        __block dispatch_semaphore_t block_sema = _renderer->_inflight_semaphore;
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
            dispatch_semaphore_signal(block_sema);
        }];
        
        // Obtain a renderPassDescriptor generated from the view's drawable textures
        MTLRenderPassDescriptor* renderPassDescriptor = _renderer->nativeView.currentRenderPassDescriptor;

        if (renderPassDescriptor) { // If we have a valid drawable, begin the commands to render into it
            // Create a render command encoder so we can render into something
            id <MTLRenderCommandEncoder> renderCommandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
            renderCommandEncoder.label = @"FSTUFF_RenderEncoder";
            [renderCommandEncoder setRenderPipelineState:_renderer->pipelineState];

            // Draw shapes
            _renderer->renderCommandEncoder = renderCommandEncoder;
            _renderer->appData = (__bridge FSTUFF_GPUData *)_renderer->gpuConstants[_renderer->constantDataBufferIndex];
//            self.sim->renderer = _renderer;
            self.sim->Render();
            
            // We're done encoding commands
            [renderCommandEncoder endEncoding];

//            // Create another render command encoder so we can render the overlay
//            id <MTLRenderCommandEncoder> overlayCommandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
//            overlayCommandEncoder.label = @"FSTUFF_OverlayRenderEncoder";
//            [overlayCommandEncoder setRenderPipelineState:_renderer->pipelineState];


            // Schedule a present once the framebuffer is complete using the current drawable
            [commandBuffer presentDrawable:_renderer->nativeView.currentDrawable];
        }

        // The render assumes it can now increment the buffer index and that the previous index won't be touched until we cycle back around to the same index
        _renderer->constantDataBufferIndex = (_renderer->constantDataBufferIndex + 1) % FSTUFF_MaxInflightBuffers;

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

