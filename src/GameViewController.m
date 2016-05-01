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

static const NSUInteger kNumberOfObjects = 2;


// Uncomment/choose only *ONE* of these
#define DRAW_CIRCLE_FILLS 1
#define DRAW_CIRCLE_EDGES 1


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
    
    // uniforms
    matrix_float4x4 _projectionMatrix;
    matrix_float4x4 _viewMatrix;
//    uniforms_t _uniform_buffer;

    id <MTLBuffer> _dynamicConstantBuffer[kMaxInflightBuffers];
    uint8_t _constantDataBufferIndex;

    //
#if DRAW_CIRCLE_FILLS
    id<MTLBuffer> _positionBuffer;
    id <MTLRenderPipelineState> _pipelineState;
#endif

#if DRAW_CIRCLE_EDGES
    id<MTLBuffer> _positionBuffer2;
    id <MTLRenderPipelineState> _pipelineState2;
#endif
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


#define VEC4_SET(V, A, B, C, D) \
    (V)[0] = (A); \
    (V)[1] = (B); \
    (V)[2] = (C); \
    (V)[3] = (D);

#define VEC4_SETRGBX(V, R, G, B, X) \
    VEC4_SET((V), ((float)(R))/255.0, ((float)(G))/255.0, ((float)(B))/255.0, ((float)(X))/255.0)

#define VEC4_SETRGB(V, R, G, B) \
    VEC4_SETRGBX(V, R, G, B, 0xff)

#define VEC4_SETRGBHEX(V, C) \
    VEC4_SETRGB(V, ((((uint32_t)(C)) >> 16) & 0xFF), ((((uint32_t)(C)) >> 8) & 0xFF), ((C) & 0xFF))

#define VEC4_SETRGBAHEX(V, C, A) \
    VEC4_SETRGBX(V, ((((uint32_t)(C)) >> 16) & 0xFF), ((((uint32_t)(C)) >> 8) & 0xFF), ((C) & 0xFF), (A))


static const unsigned kNumParts = 128;                  // REQUIRED
const float kRadStep = ((((float)M_PI) * 2.0f) / (float)kNumParts);
#define RAD_IDX(I) (((float)I) * kRadStep)
#define COS_IDX(I) cos(RAD_IDX(I))
#define SIN_IDX(I) sin(RAD_IDX(I))

#if DRAW_CIRCLE_FILLS
static const MTLPrimitiveType kPrimitiveType = MTLPrimitiveTypeTriangle;    // REQUIRED
static const unsigned kVertsPerPart = 3;
static const unsigned kNumVertices = kNumParts * kVertsPerPart;     // REQUIRED
#endif

#if DRAW_CIRCLE_EDGES
static const MTLPrimitiveType kPrimitiveType2 = MTLPrimitiveTypeTriangleStrip;   // REQUIRED
static const unsigned kNumVertices2 = 2 + (kNumParts * 2);                       // REQUIRED
static const float kInner = 0.9;
static const float kOuter = 1.0;
#endif


#if DRAW_CIRCLE_FILLS
static vector_float4 positions[kNumVertices];          // REQUIRED
#endif

#if DRAW_CIRCLE_EDGES
static vector_float4 positions2[kNumVertices2];          // REQUIRED
#endif



- (void)_loadAssets
{
    NSError *error = NULL;

    // Describe stuff common to all pipeline states
    MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.sampleCount = _view.sampleCount;
    pipelineStateDescriptor.fragmentFunction = [_defaultLibrary newFunctionWithName:@"fragment_main"];;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = _view.colorPixelFormat;

    pipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineStateDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineStateDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

#if DRAW_CIRCLE_FILLS
    // Pipeline State
    pipelineStateDescriptor.label = @"CircleFills";
    pipelineStateDescriptor.vertexFunction = [_defaultLibrary newFunctionWithName:@"vertex_circle_fill"];;
    _pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!_pipelineState) {
        NSLog(@"Failed to created pipeline state, error %@", error);
    }

    // Positions:
    for (unsigned i = 0; i < kNumParts; ++i) {
        VEC4_SET(positions[(i * kVertsPerPart) + 0],                 0,                 0, 0, 1); //i, (i*kVertsPerPart)+0);
        VEC4_SET(positions[(i * kVertsPerPart) + 1], cos(RAD_IDX( i )), sin(RAD_IDX( i )), 0, 1); //i, (i*kVertsPerPart)+1);
        VEC4_SET(positions[(i * kVertsPerPart) + 2], cos(RAD_IDX(i+1)), sin(RAD_IDX(i+1)), 0, 1); //i, (i*kVertsPerPart)+2);
    }
#endif  // DRAW_CIRCLE_FILLS


#if DRAW_CIRCLE_EDGES
    // Pipeline State
    pipelineStateDescriptor.label = @"CircleEdges";
    pipelineStateDescriptor.vertexFunction = [_defaultLibrary newFunctionWithName:@"vertex_circle_edge"];;
    _pipelineState2 = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!_pipelineState2) {
        NSLog(@"Failed to created pipeline state, error %@", error);
    }

    // Positions:
    for (unsigned i = 0; i <= kNumParts; ++i) {
        VEC4_SET(positions2[(i * 2) + 0], COS_IDX(i)*kInner, SIN_IDX(i)*kInner, 0, 1);
        VEC4_SET(positions2[(i * 2) + 1], COS_IDX(i)*kOuter, SIN_IDX(i)*kOuter, 0, 1);
    }
#endif  // DRAW_CIRCLE_EDGES


    // Setup buffers
    {
#if DRAW_CIRCLE_FILLS
        _positionBuffer = [_device newBufferWithBytes:positions
                                               length:(kNumVertices * sizeof(vector_float4))  //sizeof(positions)
                                              options:MTLResourceOptionCPUCacheModeDefault];
#endif

#if DRAW_CIRCLE_EDGES
        _positionBuffer2 = [_device newBufferWithBytes:positions2
                                                length:(kNumVertices2 * sizeof(vector_float4))  //sizeof(positions)
                                               options:MTLResourceOptionCPUCacheModeDefault];
#endif
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
    }
}

- (void)_update
{
/*
 Colors = {
	blue = "#0000ff",
	brown = "#A52A2A",
	gray = "#808080",
	green = "#00ff00",
	indigo = "#4B0082",
	light_purple = "#FF0080", -- not in html
	orange = "#FFA500",
	purple = "#800080",
	red = "#ff0000",
	turquoise = "#00ffff", -- "#40E0D0"
	violet = "#EE82EE",
	white = "#ffffff",
	yellow = "#ffff00",
 }
 
 	self.peg_colors = {
		Colors.red,
		Colors.red,
		Colors.green,
		Colors.green,
		Colors.blue,
		Colors.blue,
		Colors.yellow,
		Colors.turquoise,
	}

	self.unlit_peg_fill_alpha_min = 0.25
	self.unlit_peg_fill_alpha_max = 0.45

 pb.fill_alpha = rand_in_range(self.unlit_peg_fill_alpha_min, self.unlit_peg_fill_alpha_max)

*/

    constants_t *constant_buffer = (constants_t *)[_dynamicConstantBuffer[_constantDataBufferIndex] contents];
    for (int i = 0; i < kNumberOfObjects; i++) {
        // calculate the Model view projection matrix of each box
        constant_buffer[i].modelview_projection_matrix = matrix_from_translation(i * 0.2f, 0, 0);
        VEC4_SETRGBAHEX(constant_buffer[i].color_edge, 0xffffff, 0xff);
        VEC4_SETRGBAHEX(constant_buffer[i].color_fill, 0xffffff, 0x40);
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

#if DRAW_CIRCLE_FILLS
        [renderEncoder pushDebugGroup:@"CircleFills"];
        [renderEncoder setRenderPipelineState:_pipelineState];
        [renderEncoder setVertexBuffer:_positionBuffer offset:0 atIndex:0 ];
        [renderEncoder setVertexBuffer:_dynamicConstantBuffer[_constantDataBufferIndex] offset:0 atIndex:1 ];
        [renderEncoder drawPrimitives:kPrimitiveType
                          vertexStart:0
                          vertexCount:kNumVertices //kNumParts * kVertsPerPart  //(sizeof(positions)/sizeof(positions[0]))
                        instanceCount:kNumberOfObjects];
        [renderEncoder popDebugGroup];
#endif

#if DRAW_CIRCLE_EDGES
        [renderEncoder pushDebugGroup:@"CircleEdges"];
        [renderEncoder setRenderPipelineState:_pipelineState2];
        [renderEncoder setVertexBuffer:_positionBuffer2 offset:0 atIndex:0 ];
        [renderEncoder setVertexBuffer:_dynamicConstantBuffer[_constantDataBufferIndex] offset:0 atIndex:1 ];
        [renderEncoder drawPrimitives:kPrimitiveType2
                          vertexStart:0
                          vertexCount:kNumVertices2 //kNumParts * kVertsPerPart  //(sizeof(positions)/sizeof(positions[0]))
                        instanceCount:kNumberOfObjects];
        [renderEncoder popDebugGroup];
#endif

        
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
