//
//  FSTUFF_AppleMetalViewController.m
//  MetalTest
//
//  Created by David Ludwig on 4/30/16.
//  Copyright (c) 2018 David Ludwig. All rights reserved.
//

#include "FSTUFF.h"
#import "FSTUFF_AppleMetalViewController.h"
#import "FSTUFF_AppleMetalStructs.h"
#import "FSTUFF_AppleMetalView.h"

#if TARGET_OS_IOS
#import <GBDeviceInfo/GBDeviceInfo.h>
#endif

#pragma mark - Renderer

// The max number of command buffers in flight
static const NSUInteger kMaxInflightBuffers = 3;

// Max API memory buffer size.
static const size_t kMaxBytesPerFrame = 1024 * 1024; //64; //sizeof(vector_float4) * 3; //*1024;


void * FSTUFF_Renderer::NewVertexBuffer(void * src, size_t size)
{
    return (__bridge_retained void *)[(__bridge id <MTLDevice>)(this->device) newBufferWithBytes:src length:size options:MTLResourceOptionCPUCacheModeDefault];
}

void FSTUFF_Renderer::DestroyVertexBuffer(void * _gpuVertexBuffer)
{
    id <MTLBuffer> gpuVertexBuffer = (__bridge_transfer id <MTLBuffer>)_gpuVertexBuffer;
    gpuVertexBuffer = nil;
}


@interface FSTUFF_AppleMetalViewController()
@property (nonatomic, strong) MTKView *theView;
@end

@implementation FSTUFF_AppleMetalViewController
{
    // view
    MTKView *_view;
    
    // controller
    dispatch_semaphore_t _inflight_semaphore;
    
    // renderer
    id <MTLDevice> _device;
    id <MTLCommandQueue> _commandQueue;
    id <MTLLibrary> _defaultLibrary;
    id <MTLRenderPipelineState> _pipelineState;
    uint8_t _constantDataBufferIndex;
    id <MTLBuffer> _gpuConstants[kMaxInflightBuffers];

    // game
    FSTUFF_Renderer _renderer;
    FSTUFF_Simulation _sim;
}

- (FSTUFF_Simulation *) sim
{
    return &_sim;
}

- (void) loadView
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

    _constantDataBufferIndex = 0;
    _inflight_semaphore = dispatch_semaphore_create(3);

    // Set the view to use the default, Metal device
    _device = MTLCreateSystemDefaultDevice();

    // Create a new, Metal command queue
    _commandQueue = [_device newCommandQueue];
    
    // Load all the shader files with a metal file extension in the project
    //
    // Get the path to the bundle, in a manner that works with macOS's ScreenSaverEngine.app.
    // This app stores screensavers as separate bundles.  We need to find the bundle for the
    // screensaver that we are using.
    NSBundle * bundle = [NSBundle bundleForClass:[self class]];
    NSString * path = [bundle pathForResource:@"default" ofType:@"metallib"];
    NSError * err = nil;
    _defaultLibrary = [_device newLibraryWithFile:path error:&err];
    if ( ! _pipelineState) {
        FSTUFF_Log(@"Failed to create Metal library, error:%@\n", err);
    }

    if (_device) {
        // Setup view
        _view = (MTKView *)self.view;
        _view.delegate = self;
        _view.device = _device;
        _sim.renderer = &_renderer;

        // Describe stuff common to all pipeline states
        MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineStateDescriptor.label = @"FSTUFF_Pipeline";
        pipelineStateDescriptor.sampleCount = _view.sampleCount;
        pipelineStateDescriptor.fragmentFunction = [_defaultLibrary newFunctionWithName:@"FSTUFF_FragmentShader"];
        pipelineStateDescriptor.vertexFunction = [_defaultLibrary newFunctionWithName:@"FSTUFF_VertexShader"];
        pipelineStateDescriptor.colorAttachments[0].pixelFormat = _view.colorPixelFormat;
        pipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
        pipelineStateDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        pipelineStateDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        _pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&err];
        if ( ! _pipelineState) {
            FSTUFF_Log(@"Failed to create pipeline state, error:%@\n", err);
        }

        _renderer.device = (__bridge void *)_device;
        _renderer.nativeView = (__bridge void *)_view;

        // allocate a number of buffers in memory that matches the sempahore count so that
        // we always have one self contained memory buffer for each buffered frame.
        // In this case triple buffering is the optimal way to go so we cycle through 3 memory buffers
        for (int i = 0; i < kMaxInflightBuffers; i++) {
            _gpuConstants[i] = [_device newBufferWithLength:kMaxBytesPerFrame options:0];
            _gpuConstants[i].label = [NSString stringWithFormat:@"FSTUFF_ConstantBuffer%i", i];
        }
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
    FSTUFF_Event event = FSTUFF_NewKeyEvent(FSTUFF_EventKeyDown, [theEvent.characters cStringUsingEncoding:NSUTF8StringEncoding]);
    _sim.EventReceived(&event);
    if ( ! event.handled) {
        [super keyDown:theEvent];
    }
}
#endif

//#ifndef _MTLFeatureSet_iOS_GPUFamily3_v1

void FSTUFF_Renderer::RenderShapes(FSTUFF_Shape * shape,
                                   size_t baseInstance,
                                   size_t count,
                                   float alpha)
{
    // Metal can raise a program-crashing assertion, if zero amount of shapes attempts to get
    // rendered.
    if (count == 0) {
        return;
    }

    id <MTLDevice> gpuDevice = (__bridge id <MTLDevice>) this->device;
    id <MTLRenderCommandEncoder> renderCommandEncoder = (__bridge id <MTLRenderCommandEncoder>) this->renderer;
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
    if (baseInstance == 0) {
        [renderCommandEncoder drawPrimitives:gpuPrimitiveType
                        vertexStart:0
                        vertexCount:shape->numVertices
                      instanceCount:count];
    } else if ([gpuDevice supportsFeatureSet:featureSetForBaseInstance]) {
        [renderCommandEncoder drawPrimitives:gpuPrimitiveType
                        vertexStart:0
                        vertexCount:shape->numVertices
                      instanceCount:count
                       baseInstance:baseInstance];
    }
    [renderCommandEncoder popDebugGroup];
}

// Called whenever view changes orientation or layout is changed
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    float widthMM, heightMM;
    _renderer.nativeView = (__bridge void *)self.view;
    _renderer.GetViewSizeMM(&widthMM, &heightMM);
    _sim.ViewChanged(widthMM, heightMM);
}

// Called whenever the view needs to render
- (void)drawInMTKView:(nonnull MTKView *)view
{
    @autoreleasepool {
        dispatch_semaphore_wait(_inflight_semaphore, DISPATCH_TIME_FOREVER);
        
        // Update FSTUFF state
        if (_sim.state == FSTUFF_DEAD) {
            FSTUFF_Init(&_sim);
        }
        _renderer.appData = (FSTUFF_GPUData *) [_gpuConstants[_constantDataBufferIndex] contents];
        _sim.Update();

        // Create a new command buffer for each renderpass to the current drawable
        id <MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
        commandBuffer.label = @"FSTUFF_CommandBuffer";

        // Call the view's completion handler which is required by the view since it will signal its semaphore and set up the next buffer
        __block dispatch_semaphore_t block_sema = _inflight_semaphore;
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
            dispatch_semaphore_signal(block_sema);
        }];
        
        // Obtain a renderPassDescriptor generated from the view's drawable textures
        MTLRenderPassDescriptor* renderPassDescriptor = _view.currentRenderPassDescriptor;

        if (renderPassDescriptor) { // If we have a valid drawable, begin the commands to render into it
            // Create a render command encoder so we can render into something
            id <MTLRenderCommandEncoder> renderCommandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
            renderCommandEncoder.label = @"FSTUFF_RenderEncoder";
            [renderCommandEncoder setRenderPipelineState:_pipelineState];

            // Draw shapes
            _renderer.device = (__bridge void *)_device;
            _renderer.renderer = (__bridge void *)renderCommandEncoder;
            _renderer.appData = (__bridge FSTUFF_GPUData *)_gpuConstants[_constantDataBufferIndex];
            _sim.renderer = &_renderer;
            _sim.Render();
            
            // We're done encoding commands
            [renderCommandEncoder endEncoding];
            
            // Schedule a present once the framebuffer is complete using the current drawable
            [commandBuffer presentDrawable:_view.currentDrawable];
        }

        // The render assumes it can now increment the buffer index and that the previous index won't be touched until we cycle back around to the same index
        _constantDataBufferIndex = (_constantDataBufferIndex + 1) % kMaxInflightBuffers;

        // Finalize rendering here & push the command buffer to the GPU
        [commandBuffer commit];
    }
}

@end
