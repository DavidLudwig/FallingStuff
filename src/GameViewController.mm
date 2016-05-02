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

static matrix_float4x4 matrix_from_translation(float x, float y, float z);
static matrix_float4x4 matrix_from_scaling(float x, float y, float z);
static matrix_float4x4 matrix_from_rotation(float radians, float x, float y, float z);



// The max number of command buffers in flight
static const NSUInteger kMaxInflightBuffers = 3;

// Max API memory buffer size.
static const size_t kMaxBytesPerFrame = 1024 * 1024; //64; //sizeof(vector_float4) * 3; //*1024;

static const NSUInteger kNumberOfObjects = 2;
static const unsigned kNumCircleParts = 128;

enum FSTUFF_ShapeType : uint8_t {
    FSTUFF_ShapeCircleFilled = 0,
    FSTUFF_ShapeCircleEdged,
};

enum FSTUFF_PrimitiveType : uint8_t {
    FSTUFF_PrimitiveUnknown = 0,
    FSTUFF_PrimitiveTriangles,
    FSTUFF_PrimitiveTriangleFan,
};

struct FSTUFF_ShapeTemplate {
    const char * debugName = "";
    int numVertices = 0;
    FSTUFF_ShapeType shapeType = FSTUFF_ShapeCircleFilled;
    union {
        uint32_t _shapeGenParamsRaw = 0;
        struct {
            uint32_t numParts;
        } circle;
    };
    FSTUFF_PrimitiveType primitiveType = FSTUFF_PrimitiveUnknown;
    id <MTLBuffer> gpuVertexBuffer = nil;
};

#define RAD_IDX(I) (((float)I) * kRadStep)
#define COS_IDX(I) ((float)cos(RAD_IDX(I)))
#define SIN_IDX(I) ((float)sin(RAD_IDX(I)))

void FSTUFF_MakeCircleFilledTriangles(vector_float4 * vertices, int maxVertices, int * numVertices, int numPartsToGenerate)
{
    // TODO: check the size of the vertex buffer!
    static const int kVertsPerPart = 3;
    const float kRadStep = ((((float)M_PI) * 2.0f) / (float)numPartsToGenerate);
    *numVertices = numPartsToGenerate * kVertsPerPart;
    for (unsigned i = 0; i < numPartsToGenerate; ++i) {
        vertices[(i * kVertsPerPart) + 0] = {           0,            0, 0, 1};
        vertices[(i * kVertsPerPart) + 1] = {COS_IDX( i ), SIN_IDX( i ), 0, 1};
        vertices[(i * kVertsPerPart) + 2] = {COS_IDX(i+1), SIN_IDX(i+1), 0, 1};
    }
}

void FSTUFF_MakeCircleEdgedTriangleStrip(vector_float4 * vertices, int maxVertices, int * numVertices, int numPartsToGenerate)
{
    // TODO: check the size of the vertex buffer!
    static const float kInner = 0.9;
    static const float kOuter = 1.0;
    const float kRadStep = ((((float)M_PI) * 2.0f) / (float)numPartsToGenerate);
    *numVertices = 2 + (numPartsToGenerate * 2);
    for (unsigned i = 0; i <= numPartsToGenerate; ++i) {
        vertices[(i * 2) + 0] = {COS_IDX(i)*kInner, SIN_IDX(i)*kInner, 0, 1};
        vertices[(i * 2) + 1] = {COS_IDX(i)*kOuter, SIN_IDX(i)*kOuter, 0, 1};
    }
}

void FSTUFF_ShapeInit(FSTUFF_ShapeTemplate * shape, void * buffer, size_t bufSize, id <MTLDevice> device)
{
    // Generate vertices in CPU-accessible memory
    {
        vector_float4 * vertices = (vector_float4 *) buffer;
        const int maxElements = (int)(bufSize / sizeof(vector_float4));
        switch (shape->shapeType) {
            case FSTUFF_ShapeCircleEdged: {
                shape->primitiveType = FSTUFF_PrimitiveTriangleFan;
                FSTUFF_MakeCircleEdgedTriangleStrip(vertices, maxElements, &shape->numVertices, shape->circle.numParts);
            } break;
                
            case FSTUFF_ShapeCircleFilled: {
                shape->primitiveType = FSTUFF_PrimitiveTriangles;
                FSTUFF_MakeCircleFilledTriangles(vertices, maxElements, &shape->numVertices, shape->circle.numParts);
            } break;
        }

        shape->gpuVertexBuffer = [device newBufferWithBytes:vertices
                                                     length:(shape->numVertices * sizeof(vector_float4))
                                                    options:MTLResourceOptionCPUCacheModeDefault];
    }
}

constexpr vector_float4 FSTUFF_Color(uint32_t rgb, uint8_t a)
{
    return {
        ((((uint32_t)rgb) >> 16) & 0xFF) / 255.0f,
        ((((uint32_t)rgb) >> 8) & 0xFF) / 255.0f,
        (rgb & 0xFF) / 255.0f,
        (a) / 255.0f
    };
}

void FSTUFF_RenderShapes(FSTUFF_ShapeTemplate * shape,
                         size_t count,
                         id<MTLRenderCommandEncoder> gpuRenderer,
                         id<MTLBuffer> gpuShapeInstances,
                         vector_float4 color);


#pragma mark - Simulation

struct FSTUFF_Simulation {
    FSTUFF_ShapeTemplate circleFilled;
    FSTUFF_ShapeTemplate circleEdged;
    matrix_float4x4 projectionMatrix;
    vector_float2 viewSizeMM;
};

void FSTUFF_SimulationInit(FSTUFF_Simulation * sim, void * buffer, size_t bufSize, id<MTLDevice> gpuDevice)
{
    sim->circleFilled.debugName = "FSTUFF_CircleFills";
    sim->circleFilled.shapeType = FSTUFF_ShapeCircleFilled;
    sim->circleFilled.circle.numParts = kNumCircleParts;
    FSTUFF_ShapeInit(&(sim->circleFilled), buffer, bufSize, gpuDevice);
    
    sim->circleEdged.debugName = "FSTUFF_CircleEdges";
    sim->circleEdged.shapeType = FSTUFF_ShapeCircleEdged;
    sim->circleEdged.circle.numParts = kNumCircleParts;
    FSTUFF_ShapeInit(&(sim->circleEdged), buffer, bufSize, gpuDevice);
}

void FSTUFF_SimulationViewChanged(FSTUFF_Simulation * sim, float widthMM, float heightMM)
{
    sim->viewSizeMM = {widthMM, heightMM};
    
    matrix_float4x4 t = matrix_from_translation(-1, -1, 0);
    matrix_float4x4 s = matrix_from_scaling(2.0f / widthMM, 2.0f / heightMM, 1); //100.0f / w, 100.0f / h, 1.0f);
    sim->projectionMatrix = matrix_multiply(t, s);
}

void FSTUFF_SimulationUpdate(FSTUFF_Simulation * sim, FSTUFF_ShapeInstance * shapeInstances)
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

    for (int i = 0; i < kNumberOfObjects; i++) {
        shapeInstances[i].modelview_projection_matrix = \
            matrix_multiply(
                sim->projectionMatrix,
                matrix_from_translation(
                    (sim->viewSizeMM[0] / 2.0f) + (10.0f * (float)i),
                    (sim->viewSizeMM[1] / 2.0f),
                    0
                )
            );
    }
}

void FSTUFF_SimulationRender(FSTUFF_Simulation * sim,
                             id <MTLRenderCommandEncoder> gpuRenderer,
                             id <MTLBuffer> gpuShapeInstances)
{
    FSTUFF_RenderShapes(&sim->circleFilled, kNumberOfObjects, gpuRenderer, gpuShapeInstances, FSTUFF_Color(0xffffff, 0x40));
    FSTUFF_RenderShapes(&sim->circleEdged,  kNumberOfObjects, gpuRenderer, gpuShapeInstances, FSTUFF_Color(0xffffff, 0xff));
}



#pragma mark - Renderer

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
    id <MTLBuffer> _gpuShapeInstances[kMaxInflightBuffers];
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

    uint8_t tempBufferForInit[64 * 1024];
    FSTUFF_SimulationInit(&_sim, tempBufferForInit, sizeof(tempBufferForInit), _device);

    // allocate a number of buffers in memory that matches the sempahore count so that
    // we always have one self contained memory buffer for each buffered frame.
    // In this case triple buffering is the optimal way to go so we cycle through 3 memory buffers
    for (int i = 0; i < kMaxInflightBuffers; i++) {
        _gpuShapeInstances[i] = [_device newBufferWithLength:kMaxBytesPerFrame options:0];
        _gpuShapeInstances[i].label = [NSString stringWithFormat:@"FSTUFF_ConstantBuffer%i", i];
    }
}

- (void)_update
{
    FSTUFF_ShapeInstance *constant_buffer = (FSTUFF_ShapeInstance *)[_gpuShapeInstances[_constantDataBufferIndex] contents];
    FSTUFF_SimulationUpdate(&_sim, constant_buffer);
}

void FSTUFF_RenderShapes(FSTUFF_ShapeTemplate * shape,
                         size_t count,
                         id <MTLRenderCommandEncoder> gpuRenderer,
                         id <MTLBuffer> gpuShapeInstances,
                         vector_float4 color)
{
    MTLPrimitiveType gpuPrimitiveType;
    switch (shape->primitiveType) {
        case FSTUFF_PrimitiveTriangles:
            gpuPrimitiveType = MTLPrimitiveTypeTriangle;
            break;
        case FSTUFF_PrimitiveTriangleFan:
            gpuPrimitiveType = MTLPrimitiveTypeTriangleStrip;
            break;
        default:
            NSLog(@"Unknown or unset FSTUFF_PrimitiveType in shape: %u", shape->primitiveType);
            return;
    }
    
    [gpuRenderer pushDebugGroup:[NSString stringWithUTF8String:shape->debugName]];
    [gpuRenderer setVertexBuffer:shape->gpuVertexBuffer offset:0 atIndex:0];
    [gpuRenderer setVertexBuffer:gpuShapeInstances offset:0 atIndex:1];
    [gpuRenderer setVertexBytes:&color length:sizeof(color) atIndex:2];
    
    [gpuRenderer drawPrimitives:gpuPrimitiveType
                    vertexStart:0
                    vertexCount:shape->numVertices
                  instanceCount:count];
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
        FSTUFF_SimulationRender(&_sim, gpuRenderer, _gpuShapeInstances[_constantDataBufferIndex]);
        
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

    NSScreen * screen = self.view.window.screen;
    NSNumber * displayID = [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
    CGDirectDisplayID cgDisplayID = (CGDirectDisplayID) [displayID intValue];
    const CGSize scrSizeMM = CGDisplayScreenSize(cgDisplayID);
    const CGSize ptsToMM = CGSizeMake(scrSizeMM.width / screen.frame.size.width,
                                      scrSizeMM.height / screen.frame.size.height);
    const CGSize viewSizeMM = CGSizeMake(self.view.bounds.size.width * ptsToMM.width,
                                         self.view.bounds.size.height * ptsToMM.height);

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
