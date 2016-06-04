//
//  GameViewController.m
//  MetalTest
//
//  Created by David Ludwig on 4/30/16.
//  Copyright (c) 2016 David Ludwig. All rights reserved.
//

#import "GameViewController.h"
#import <Metal/Metal.h>
#import <simd/simd.h>
#import <MetalKit/MetalKit.h>
#import "SharedStructures.h"
#include <sys/time.h>   // for gettimeofday()
#include <random>
#include "FSTUFF_Colors.h"

#include "FSTUFF_Simulation.h"

#if TARGET_OS_IOS
#import <GBDeviceInfo/GBDeviceInfo.h>
#endif

#pragma mark - Renderer

// The max number of command buffers in flight
static const NSUInteger kMaxInflightBuffers = 3;

// Max API memory buffer size.
static const size_t kMaxBytesPerFrame = 1024 * 1024; //64; //sizeof(vector_float4) * 3; //*1024;


void * FSTUFF_VertexBufferNew(void * gpuDevice, void * src, size_t size)
{
    return (__bridge_retained void *)[(__bridge id <MTLDevice>)gpuDevice newBufferWithBytes:src length:size options:MTLResourceOptionCPUCacheModeDefault];
}

void FSTUFF_VertexBufferDestroy(void * _gpuVertexBuffer)
{
    id <MTLBuffer> gpuVertexBuffer = (__bridge_transfer id <MTLBuffer>)_gpuVertexBuffer;
    gpuVertexBuffer = nil;
}


@interface GameViewController()
@property (nonatomic, strong) MTKView *theView;
@end

@implementation GameViewController
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
    
    // game
    FSTUFF_Simulation _sim;
    id <MTLBuffer> _gpuConstants[kMaxInflightBuffers];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    _constantDataBufferIndex = 0;
    _inflight_semaphore = dispatch_semaphore_create(3);
    
    [self _setupMetal];
    if(_device)
    {
        [self _setupView];
        [self _reshape];
        [self _loadAssets];
    }
    else // Fallback to a blank NSView, an application could also fallback to OpenGL here.
    {
        NSLog(@"Metal is not supported on this device");
#if ! TARGET_OS_IOS
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
#endif
    }
}

- (void)_setupView
{
    _view = (MTKView *)self.view;
    _view.delegate = self;
    _view.device = _device;
}

- (void)_setupMetal
{
    // Set the view to use the default device
    _device = MTLCreateSystemDefaultDevice();

    // Create a new command queue
    _commandQueue = [_device newCommandQueue];
    
    // Load all the shader files with a metal file extension in the project
    _defaultLibrary = [_device newDefaultLibrary];
}

- (void)_loadAssets
{
    NSError * error = NULL;

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
    _pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if ( ! _pipelineState) {
        NSLog(@"Failed to created pipeline state, error %@", error);
    }

    FSTUFF_SimulationInit(&_sim, (__bridge void *)_device);

    // allocate a number of buffers in memory that matches the sempahore count so that
    // we always have one self contained memory buffer for each buffered frame.
    // In this case triple buffering is the optimal way to go so we cycle through 3 memory buffers
    for (int i = 0; i < kMaxInflightBuffers; i++) {
        _gpuConstants[i] = [_device newBufferWithLength:kMaxBytesPerFrame options:0];
        _gpuConstants[i].label = [NSString stringWithFormat:@"FSTUFF_ConstantBuffer%i", i];
    }
}

#if ! TARGET_OS_IOS
- (void)keyDown:(NSEvent *)theEvent
{
    FSTUFF_Event evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = FSTUFF_EventKeyDown;
    evt.key.utf8 = [theEvent.characters cStringUsingEncoding:NSUTF8StringEncoding];
    FSTUFF_SimulationEvent(&_sim, &evt);
}
#endif

- (void)_update
{
    const uint8_t * constants = (uint8_t *) [_gpuConstants[_constantDataBufferIndex] contents];
    FSTUFF_GPUData * gpuData = (FSTUFF_GPUData *) constants;
    FSTUFF_SimulationUpdate(&_sim, gpuData);
}

//#ifndef _MTLFeatureSet_iOS_GPUFamily3_v1

void FSTUFF_RenderShapes(FSTUFF_ShapeTemplate * shape,
                         size_t baseInstance,
                         size_t count,
                         void * _gpuDevice,
                         void * _gpuRenderer,
                         void * _gpuData,    // laid out as a 'FSTUFF_GPUData' struct
                         float alpha)
{
    id <MTLDevice> gpuDevice = (__bridge id <MTLDevice>)_gpuDevice;
    id <MTLRenderCommandEncoder> gpuRenderer = (__bridge id <MTLRenderCommandEncoder>)_gpuRenderer;
    id <MTLBuffer> gpuData = (__bridge id <MTLBuffer>)_gpuData;

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
            NSLog(@"Unknown or unmapped FSTUFF_PrimitiveType in shape: %u", shape->primitiveType);
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
            NSLog(@"Unknown or unmapped FSTUFF_ShapeType in shape: %u", shape->type);
            return;
    }
    
    [gpuRenderer pushDebugGroup:[NSString stringWithUTF8String:shape->debugName]];
    [gpuRenderer setVertexBuffer:(__bridge id <MTLBuffer>)shape->gpuVertexBuffer offset:0 atIndex:0];                    // 'position[<vertex id>]'
    [gpuRenderer setVertexBuffer:gpuData offset:offsetof(FSTUFF_GPUData, globals) atIndex:1];   // 'gpuGlobals'
    [gpuRenderer setVertexBuffer:gpuData offset:shapesOffsetInGpuData atIndex:2];               // 'gpuShapes[<instance id>]'
    [gpuRenderer setVertexBytes:&alpha length:sizeof(alpha) atIndex:3];                         // 'alpha'
    
#if TARGET_OS_IOS
    const MTLFeatureSet featureSetForBaseInstance = MTLFeatureSet_iOS_GPUFamily3_v1;
#else
    const MTLFeatureSet featureSetForBaseInstance = MTLFeatureSet_OSX_GPUFamily1_v1;
#endif
    if (baseInstance == 0) {
        [gpuRenderer drawPrimitives:gpuPrimitiveType
                        vertexStart:0
                        vertexCount:shape->numVertices
                      instanceCount:count];
    } else if ([gpuDevice supportsFeatureSet:featureSetForBaseInstance]) {
        [gpuRenderer drawPrimitives:gpuPrimitiveType
                        vertexStart:0
                        vertexCount:shape->numVertices
                      instanceCount:count
                       baseInstance:baseInstance];
    }
    [gpuRenderer popDebugGroup];
}

- (void)_render
{
    dispatch_semaphore_wait(_inflight_semaphore, DISPATCH_TIME_FOREVER);
    
    [self _update];

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

    if(renderPassDescriptor != nil) // If we have a valid drawable, begin the commands to render into it
    {
        // Create a render command encoder so we can render into something
        id <MTLRenderCommandEncoder> gpuRenderer = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        gpuRenderer.label = @"FSTUFF_RenderEncoder";
        [gpuRenderer setRenderPipelineState:_pipelineState];

        // Draw shapes
        FSTUFF_SimulationRender(&_sim, (__bridge void *)_device, (__bridge void *)gpuRenderer, (__bridge void *)_gpuConstants[_constantDataBufferIndex]);
        
        // We're done encoding commands
        [gpuRenderer endEncoding];
        
        // Schedule a present once the framebuffer is complete using the current drawable
        [commandBuffer presentDrawable:_view.currentDrawable];
    }

    // The render assumes it can now increment the buffer index and that the previous index won't be touched until we cycle back around to the same index
    _constantDataBufferIndex = (_constantDataBufferIndex + 1) % kMaxInflightBuffers;

    // Finalize rendering here & push the command buffer to the GPU
    [commandBuffer commit];
}

- (void)_reshape
{
//    // When reshape is called, update the view and projection matricies since this means the view orientation or size changed

#if TARGET_OS_IOS
    UIScreen * screen = [UIScreen mainScreen]; //self.view.window.screen;
    const float scale = [screen scale];
    const float ppi = [GBDeviceInfo deviceInfo].displayInfo.pixelsPerInch;
    const float width = ([screen bounds].size.width * scale);
    const float height = ([screen bounds].size.height * scale);
    const CGSize viewSizeMM = CGSizeMake((width / ppi) * 25.4f,
                                         (height / ppi) * 25.4f);

#else
    NSScreen * screen = self.view.window.screen;
    NSNumber * displayID = [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
    CGDirectDisplayID cgDisplayID = (CGDirectDisplayID) [displayID intValue];
    const CGSize scrSizeMM = CGDisplayScreenSize(cgDisplayID);
    const CGSize ptsToMM = CGSizeMake(scrSizeMM.width / screen.frame.size.width,
                                      scrSizeMM.height / screen.frame.size.height);
    const CGSize viewSizeMM = CGSizeMake(self.view.bounds.size.width * ptsToMM.width,
                                         self.view.bounds.size.height * ptsToMM.height);

#endif

//    NSLog(@"view size: {%f,%f} (pts?) --> {%f,%f} (mm?)\n",
//          self.view.bounds.size.width,
//          self.view.bounds.size.height,
//          viewSizeMM.width,
//          viewSizeMM.height);
    
    FSTUFF_SimulationViewChanged(&_sim, viewSizeMM.width, viewSizeMM.height);
}

// Called whenever view changes orientation or layout is changed
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    [self _reshape];
}

// Called whenever the view needs to render
- (void)drawInMTKView:(nonnull MTKView *)view
{
    @autoreleasepool {
        [self _render];
    }
}

@end
