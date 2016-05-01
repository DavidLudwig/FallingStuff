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

// The max number of command buffers in flight
static const NSUInteger kMaxInflightBuffers = 3;

// Max API memory buffer size.
static const size_t kMaxBytesPerFrame = 1024*1024;

static const NSUInteger kNumberOfObjects = 1;


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
    
    // uniforms
    matrix_float4x4 _projectionMatrix;
    matrix_float4x4 _viewMatrix;
//    uniforms_t _uniform_buffer;

    id <MTLBuffer> _dynamicConstantBuffer[kMaxInflightBuffers];
    uint8_t _constantDataBufferIndex;
    
    // buffers
    id<MTLBuffer> positionBuffer;
    id<MTLBuffer> colorBuffer;
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
        [self _loadAssets];
        [self _reshape];
    }
    else // Fallback to a blank NSView, an application could also fallback to OpenGL here.
    {
        NSLog(@"Metal is not supported on this device");
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
    }
}

- (void)_setupView
{
    [MTKView class];    // HACK from DavidL, 2016-04-30: make sure the MetalKit's
                        // shared library gets loaded at runtime.  It looks like
                        // MetalKit won't get loaded unless certain methods,
                        // seemingly global ones (such as class methods), get.
                        // invoked.  This is/was true with the following build
                        // setup:
                        //
                        // * Xcode 7.3
                        // * Mac OS X SDK 10.11 (included as part of Xcode 7.3)
                        // * Build machine running OS X 10.11.4
                        //
                        // Failure to use the hack caused self.view to be an
                        // NSView, which led to unrecognized selector errors
                        // (when setting MTKView properties).
//    NSLog(@"%@", NSStringFromClass([self.view class]));

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

//static const MTLPrimitiveType kPrimitiveType = MTLPrimitiveTypeTriangle;
//
//static const vector_float4 positions[] =
//{
//    {0.0,  0.5, 0, 1},
//    {-0.5, -0.5, 0, 1},
//    {0.5, -0.5, 0, 1},
//};
//
//static const vector_float4 colors[] =
//{
//    {1, 0, 0, 1},
//    {0, 1, 0, 1},
//    {0, 0, 1, 1},
//};


//static const vector_float4 positions[] =
//{
////    {0,     0,      0},
////    {0,   0,   0},
////    {
//
////    { 0.0,  0.5, 0, 1},
////    {-1.0, -0.5, 0, 1},
////    { 0.5, -1.0, 0, 1},
////    {-0.8, -0.8, 0, 1},
//    
//    {   0,    0,        0, 1},
//    {   0,  0.5,        0, 1},
//    { 0.3,  0.3,        0, 1},
//
//    {   0,    0,        0, 1},
//    { 0.3,  0.3,        0, 1},
//    { 0.5,    0,        0, 1},
//
//};
//
//static const vector_float4 colors[] =
//{
//    {1, 0, 0, 1},
//    {0, 1, 0, 1},
//    {0, 0, 1, 1},
//    {1, 0, 0, 1},
//    {0, 1, 0, 1},
//    {0, 0, 1, 1},
//};


#define VEC4_SET(V, A, B, C, D) \
    (V)[0] = (A); \
    (V)[1] = (B); \
    (V)[2] = (C); \
    (V)[3] = (D);

#define VEC4_SETRGBX(V, R, G, B, X) \
    VEC4_SET((V), ((float)(R))/255.0, ((float)(G))/255.0, ((float)(B))/255.0, ((float)(B))/255.0)

#define VEC4_SETRGB(V, R, G, B) \
    VEC4_SETRGBX(V, R, G, B, 0xff)

#define VEC4_SETRGBHEX(V, C) \
    VEC4_SETRGB(V, ((((uint32_t)(C)) >> 16) & 0xFF), ((((uint32_t)(C)) >> 8) & 0xFF), ((C) & 0xFF))


// Uncomment/choose only *ONE* of these
//#define DRAW_CIRCLE_FILLED 1
#define DRAW_CIRCLE_EDGED  1

static const unsigned kNumParts = 128;                  // REQUIRED
const float kRadStep = ((((float)M_PI) * 2.0f) / (float)kNumParts);
#define RAD_IDX(I) (((float)I) * kRadStep)
#define COS_IDX(I) cos(RAD_IDX(I))
#define SIN_IDX(I) sin(RAD_IDX(I))

#if DRAW_CIRCLE_FILLED
static const MTLPrimitiveType kPrimitiveType = MTLPrimitiveTypeTriangle;    // REQUIRED
static const unsigned kVertsPerPart = 3;
static const unsigned kNumVertices = kNumParts * kVertsPerPart;     // REQUIRED
#endif

#if DRAW_CIRCLE_EDGED
static const MTLPrimitiveType kPrimitiveType = MTLPrimitiveTypeTriangleStrip;   // REQUIRED
static const unsigned kNumVertices = 2 + (kNumParts * 2);                       // REQUIRED
static const float kInner = 0.9;
static const float kOuter = 1.0;
#endif

static vector_float4 positions[kNumVertices];          // REQUIRED
static vector_float4 colors[kNumVertices];             // REQUIRED


- (void)_loadAssets
{
#if DRAW_CIRCLE_EDGED
    // Positions:
    for (unsigned i = 0; i <= kNumParts; ++i) {
        VEC4_SET(positions[(i * 2) + 0], COS_IDX(i)*kInner, SIN_IDX(i)*kInner, 0, 1);
        VEC4_SET(positions[(i * 2) + 1], COS_IDX(i)*kOuter, SIN_IDX(i)*kOuter, 0, 1);
    }
    
    // Colors:
#if 0
    for (unsigned i = 0; i < kNumVertices; ++i) {
        VEC4_SETRGBHEX(colors[i], 0xFBFFC2);
    }
#else
    for (unsigned i = 0; i < kNumParts; ++i) {
        vector_float4 c = {1, 1, 1, 1};
        switch (i % 4) {
            case 0:
                VEC4_SET(c, 1, 0, 0, 1);
                break;
            case 1:
                VEC4_SET(c, 0, 1, 0, 1);
                break;
            case 2:
                VEC4_SET(c, 0, 0, 1, 1);
                break;
            case 3:
                VEC4_SET(c, 1, 1, 0, 1);
                break;
        }
        for (unsigned j = 0; j < 2; ++j) {
            colors[(i * 2) + j] = c;
        }
    }
#endif
#endif  // DRAW_CIRCLE_EDGED

#if DRAW_CIRCLE_FILLED
    // Positions:
    for (unsigned i = 0; i < kNumParts; ++i) {
        VEC4_SET(positions[(i * kVertsPerPart) + 0],                 0,                 0, 0, 1); //i, (i*kVertsPerPart)+0);
        VEC4_SET(positions[(i * kVertsPerPart) + 1], cos(RAD_IDX( i )), sin(RAD_IDX( i )), 0, 1); //i, (i*kVertsPerPart)+1);
        VEC4_SET(positions[(i * kVertsPerPart) + 2], cos(RAD_IDX(i+1)), sin(RAD_IDX(i+1)), 0, 1); //i, (i*kVertsPerPart)+2);
    }

    // Colors:
    for (unsigned i = 0; i < kNumParts; ++i) {
        vector_float4 c = {1, 1, 1, 1};
        switch (i % 4) {
            case 0:
                VEC4_SET(c, 1, 0, 0, 1);
                break;
            case 1:
                VEC4_SET(c, 0, 1, 0, 1);
                break;
            case 2:
                VEC4_SET(c, 0, 0, 1, 1);
                break;
            case 3:
                VEC4_SET(c, 1, 1, 0, 1);
                break;
        }
        for (unsigned j = 0; j < kVertsPerPart; ++j) {
            colors[(i * kVertsPerPart) + j] = c;
        }
    }
#endif  // DRAW_CIRCLE_FILLED


    // Setup buffers
    {
        positionBuffer = [_device newBufferWithBytes:positions
                                              length:(kNumVertices * sizeof(vector_float4))  //sizeof(positions)
                                             options:MTLResourceOptionCPUCacheModeDefault];
        colorBuffer = [_device newBufferWithBytes:colors
                                           length:(kNumVertices * sizeof(vector_float4))  //sizeof(colors)
                                          options:MTLResourceOptionCPUCacheModeDefault];
    }
    
//    // Allocate one region of memory for the uniform buffer
//    _dynamicConstantBuffer = [_device newBufferWithLength:kMaxBytesPerFrame options:0];
//    _dynamicConstantBuffer.label = @"UniformBuffer";

    // allocate a number of buffers in memory that matches the sempahore count so that
    // we always have one self contained memory buffer for each buffered frame.
    // In this case triple buffering is the optimal way to go so we cycle through 3 memory buffers
    for (int i = 0; i < kMaxInflightBuffers; i++)
    {
        _dynamicConstantBuffer[i] = [_device newBufferWithLength:kMaxBytesPerFrame options:0];
        _dynamicConstantBuffer[i].label = [NSString stringWithFormat:@"ConstantBuffer%i", i];
        
        // write initial color values for both cubes (at each offset).
        // Note, these will get animated during update
//        constants_t *constant_buffer = (constants_t *)[_dynamicConstantBuffer[i] contents];
//        for (int j = 0; j < kNumberOfObjects; j++)
//        {
//            if (j%2==0) {
//                constant_buffer[j].multiplier = 1;
//                constant_buffer[j].ambient_color = kBoxAmbientColors[0];
//                constant_buffer[j].diffuse_color = kBoxDiffuseColors[0];
//            }
//            else {
//                constant_buffer[j].multiplier = -1;
//                constant_buffer[j].ambient_color = kBoxAmbientColors[1];
//                constant_buffer[j].diffuse_color = kBoxDiffuseColors[1];
//            }
//        }
    }
    
    // Load the fragment program into the library
    id <MTLFunction> fragmentProgram = [_defaultLibrary newFunctionWithName:@"fragment_main"]; //lighting_fragment"];
    
    // Load the vertex program into the library
    id <MTLFunction> vertexProgram = [_defaultLibrary newFunctionWithName:@"vertex_main"]; //lighting_vertex"];
    
    // Create a reusable pipeline state
    MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.label = @"MyPipeline";
    pipelineStateDescriptor.sampleCount = _view.sampleCount;
    pipelineStateDescriptor.vertexFunction = vertexProgram;
    pipelineStateDescriptor.fragmentFunction = fragmentProgram;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = _view.colorPixelFormat;
    
    NSError *error = NULL;
    _pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!_pipelineState) {
        NSLog(@"Failed to created pipeline state, error %@", error);
    }
}

- (void)_render
{
    dispatch_semaphore_wait(_inflight_semaphore, DISPATCH_TIME_FOREVER);
    
    [self _update];

    // Create a new command buffer for each renderpass to the current drawable
    id <MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    commandBuffer.label = @"MyCommand";

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
        id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderEncoder.label = @"MyRenderEncoder";
        
        // Set context state
        [renderEncoder pushDebugGroup:@"DrawStuff"];
        [renderEncoder setRenderPipelineState:_pipelineState];
        [renderEncoder setVertexBuffer:positionBuffer offset:0 atIndex:0 ];
        [renderEncoder setVertexBuffer:colorBuffer offset:0 atIndex:1 ];
        [renderEncoder setVertexBuffer:_dynamicConstantBuffer[_constantDataBufferIndex] offset:0 atIndex:2 ];
        [renderEncoder drawPrimitives:kPrimitiveType
                          vertexStart:0
                          vertexCount:kNumVertices //kNumParts * kVertsPerPart  //(sizeof(positions)/sizeof(positions[0]))
                        instanceCount:kNumberOfObjects];
        [renderEncoder popDebugGroup];
        
        // We're done encoding commands
        [renderEncoder endEncoding];
        
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
//    float aspect = fabs(self.view.bounds.size.width / self.view.bounds.size.height);
//    _projectionMatrix = matrix_from_perspective_fov_aspectLH(65.0f * (M_PI / 180.0f), aspect, 0.1f, 100.0f);
//    _viewMatrix = matrix_identity_float4x4;
}

- (void)_update
{
    constants_t *constant_buffer = (constants_t *)[_dynamicConstantBuffer[_constantDataBufferIndex] contents];
    for (int i = 0; i < kNumberOfObjects; i++) {
        // calculate the Model view projection matrix of each box
        constant_buffer[i].modelview_projection_matrix = matrix_from_translation(i * 0.2f, 0, 0);
        VEC4_SETRGBHEX(constant_buffer[i].color, 0xB19807);
    }
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

#pragma mark Utilities

static matrix_float4x4 matrix_from_translation(float x, float y, float z)
{
    matrix_float4x4 m = matrix_identity_float4x4;
    m.columns[3] = (vector_float4) { x, y, z, 1.0 };
    return m;
}

static matrix_float4x4 matrix_from_scaling(float x, float y, float z)
{
    matrix_float4x4 m = matrix_identity_float4x4;
    m.columns[0][0] = x;
    m.columns[1][1] = y;
    m.columns[2][2] = z;
    m.columns[3][3] = 1.0f;
    return m;
}

static matrix_float4x4 matrix_from_rotation(float radians, float x, float y, float z)
{
    vector_float3 v = vector_normalize(((vector_float3){x, y, z}));
    float cos = cosf(radians);
    float cosp = 1.0f - cos;
    float sin = sinf(radians);
    
    matrix_float4x4 m = {
        .columns[0] = {
            cos + cosp * v.x * v.x,
            cosp * v.x * v.y + v.z * sin,
            cosp * v.x * v.z - v.y * sin,
            0.0f,
        },
        
        .columns[1] = {
            cosp * v.x * v.y - v.z * sin,
            cos + cosp * v.y * v.y,
            cosp * v.y * v.z + v.x * sin,
            0.0f,
        },
        
        .columns[2] = {
            cosp * v.x * v.z + v.y * sin,
            cosp * v.y * v.z - v.x * sin,
            cos + cosp * v.z * v.z,
            0.0f,
        },
        
        .columns[3] = { 0.0f, 0.0f, 0.0f, 1.0f
        }
    };
    return m;
}

@end
