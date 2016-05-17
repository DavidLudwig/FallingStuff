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

extern "C" {
//#include <chipmunk/chipmunk_structs.h>
#include <chipmunk/chipmunk_private.h>  // #include'd solely for allowing static cp* structs (cpSpace, cpBody, etc.)
}

#include <chipmunk/chipmunk.h>

static matrix_float4x4 matrix_from_translation(float x, float y, float z);
static matrix_float4x4 matrix_from_scaling(float x, float y, float z);
static matrix_float4x4 matrix_from_rotation(float radians, float x, float y, float z);



// The max number of command buffers in flight
static const NSUInteger kMaxInflightBuffers = 3;

// Max API memory buffer size.
static const size_t kMaxBytesPerFrame = 1024 * 1024; //64; //sizeof(vector_float4) * 3; //*1024;

static const unsigned kNumCircleParts = 128;

enum FSTUFF_ShapeType : uint8_t {
    FSTUFF_ShapeCircle = 0,
    FSTUFF_ShapeBox,
};

enum FSTUFF_ShapeAppearance : uint8_t {
    FSTUFF_ShapeAppearanceFilled = 0,
    FSTUFF_ShapeAppearanceEdged,
};

enum FSTUFF_PrimitiveType : uint8_t {
    FSTUFF_PrimitiveUnknown = 0,
    FSTUFF_PrimitiveLineStrip,
    FSTUFF_PrimitiveTriangles,
    FSTUFF_PrimitiveTriangleFan,
};

struct FSTUFF_ShapeTemplate {
    const char * debugName = "";
    int numVertices = 0;
    FSTUFF_ShapeType type = FSTUFF_ShapeCircle;
    FSTUFF_ShapeAppearance appearance = FSTUFF_ShapeAppearanceFilled;
    union {
        uint32_t _shapeGenParamsRaw = 0;
        struct {
            uint32_t numParts;
        } circle;
    };
    FSTUFF_PrimitiveType primitiveType = FSTUFF_PrimitiveUnknown;
    id <MTLBuffer> gpuVertexBuffer = nil;
};

#define RAD_IDX(I) (((float)I) * kRadianStep)
#define COS_IDX(I) ((float)cos(RAD_IDX(I)))
#define SIN_IDX(I) ((float)sin(RAD_IDX(I)))

void FSTUFF_MakeCircleFilledTriangles(vector_float4 * vertices, int maxVertices, int * numVertices, int numPartsToGenerate)
{
    // TODO: check the size of the vertex buffer!
    static const int kVertsPerPart = 3;
    const float kRadianStep = ((((float)M_PI) * 2.0f) / (float)numPartsToGenerate);
    *numVertices = numPartsToGenerate * kVertsPerPart;
    for (unsigned i = 0; i < numPartsToGenerate; ++i) {
        vertices[(i * kVertsPerPart) + 0] = {           0,            0, 0, 1};
        vertices[(i * kVertsPerPart) + 1] = {COS_IDX( i ), SIN_IDX( i ), 0, 1};
        vertices[(i * kVertsPerPart) + 2] = {COS_IDX(i+1), SIN_IDX(i+1), 0, 1};
    }
}

void FSTUFF_MakeCircleTriangleStrip(vector_float4 * vertices, int maxVertices, int * numVertices, int numPartsToGenerate,
                                    float innerRadius, float outerRadius)
{
    // TODO: check the size of the vertex buffer!
    const float kRadianStep = ((((float)M_PI) * 2.0f) / (float)numPartsToGenerate);
    *numVertices = 2 + (numPartsToGenerate * 2);
    for (unsigned i = 0; i <= numPartsToGenerate; ++i) {
        vertices[(i * 2) + 0] = {COS_IDX(i)*innerRadius, SIN_IDX(i)*innerRadius, 0, 1};
        vertices[(i * 2) + 1] = {COS_IDX(i)*outerRadius, SIN_IDX(i)*outerRadius, 0, 1};
    }
}

void FSTUFF_MakeCircleLineStrip(vector_float4 * vertices, int maxVertices, int * numVertices, int numPartsToGenerate,
                                float radius)
{
    // TODO: check the size of the vertex buffer!
    const float kRadianStep = ((((float)M_PI) * 2.0f) / (float)numPartsToGenerate);
    *numVertices = (numPartsToGenerate + 1);
    for (unsigned i = 0; i <= numPartsToGenerate; ++i) {
        vertices[i] = {COS_IDX(i)*radius, SIN_IDX(i)*radius, 0, 1};
    }
}


void FSTUFF_ShapeInit(FSTUFF_ShapeTemplate * shape, void * buffer, size_t bufSize, id <MTLDevice> device)
{
    // Generate vertices in CPU-accessible memory
    vector_float4 * vertices = (vector_float4 *) buffer;
    const int maxElements = (int)(bufSize / sizeof(vector_float4));
    bool didSet = false;
    
    //
    // Circles
    //
    if (shape->type == FSTUFF_ShapeCircle) {
        if (shape->appearance == FSTUFF_ShapeAppearanceEdged) {
            didSet = true;
#if 0
            shape->primitiveType = FSTUFF_PrimitiveTriangleFan;
            FSTUFF_MakeCircleTriangleStrip(vertices, maxElements, &shape->numVertices, shape->circle.numParts,
                                           0.9,     // inner radius
                                           1.0);    // outer radius
#else
            shape->primitiveType = FSTUFF_PrimitiveLineStrip;
            FSTUFF_MakeCircleLineStrip(vertices, maxElements, &shape->numVertices, shape->circle.numParts,
                                       1.0);        // radius
#endif
        } else if (shape->appearance == FSTUFF_ShapeAppearanceFilled) {
            didSet = true;
            shape->primitiveType = FSTUFF_PrimitiveTriangles;
            FSTUFF_MakeCircleFilledTriangles(vertices, maxElements, &shape->numVertices, shape->circle.numParts);
        }
    }
    
    //
    // Boxes
    //
    else if (shape->type == FSTUFF_ShapeBox) {
        if (shape->appearance == FSTUFF_ShapeAppearanceEdged) {
            didSet = true;
            shape->primitiveType = FSTUFF_PrimitiveLineStrip;
            shape->numVertices = 5;
            vertices[0] = {-.5f,  .5f,  0, 1};
            vertices[1] = { .5f,  .5f,  0, 1};
            vertices[2] = { .5f, -.5f,  0, 1};
            vertices[3] = {-.5f, -.5f,  0, 1};
            vertices[4] = {-.5f,  .5f,  0, 1};
        } else if (shape->appearance == FSTUFF_ShapeAppearanceFilled) {
            didSet = true;
            shape->primitiveType = FSTUFF_PrimitiveTriangleFan;
            shape->numVertices = 4;
            vertices[0] = {-.5f, -.5f, 0, 1};
            vertices[1] = {-.5f,  .5f, 0, 1};
            vertices[2] = { .5f, -.5f, 0, 1};
            vertices[3] = { .5f,  .5f, 0, 1};
        }
    }

    if (didSet) {
        shape->gpuVertexBuffer = [device newBufferWithBytes:vertices
                                                     length:(shape->numVertices * sizeof(vector_float4))
                                                    options:MTLResourceOptionCPUCacheModeDefault];
    } else {
        shape->gpuVertexBuffer = nil;
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

constexpr vector_float4 FSTUFF_Color(uint32_t rgb)
{
    return FSTUFF_Color(rgb, 0xff);
}


void FSTUFF_RenderShapes(FSTUFF_ShapeTemplate * shape,
                         size_t count,
                         id<MTLRenderCommandEncoder> gpuRenderer,
                         id<MTLBuffer> gpuAppData,
                         float alpha);


#pragma mark - Simulation

struct FSTUFF_Simulation {
    //
    // Geometry + GPU
    //
    FSTUFF_ShapeTemplate circleFilled;
    FSTUFF_ShapeTemplate circleEdged;
    FSTUFF_ShapeTemplate boxFilled;
    FSTUFF_ShapeTemplate boxEdged;
    matrix_float4x4 projectionMatrix;
    //vector_float2 viewSizeMM;
    cpVect viewSizeMM;

    //
    // Timing
    //
    cpFloat lastUpdateUTCTimeS = 0.0;     // set on FSTUFF_Update; UTC time in seconds
    
    //
    // Physics
    //
    // HACK: C++ won't allow us to create a 'cpSpace' directly, due to constructor issues,
    //   so we'll allocate one ourselves.
    //
    //   To note, Chipmunk Physics' cpAlloc() function just calls calloc() anyways,
    //   and according to its docs, using custom allocation, as is done here, is ok.
    //   Further, zero-ing out the memory is not required (again, according to the docs).
    uint8_t _physicsSpaceStorage[sizeof(cpSpace)];
    cpSpace * physicsSpace = (cpSpace *) &_physicsSpaceStorage;

    size_t numCircles = 0;
    cpCircleShape circles[kMaxCircles];
    vector_float4 circleColors[kMaxCircles];

    size_t numBoxes;
    cpSegmentShape boxes[kMaxBoxes];
    vector_float4 boxColors[kMaxBoxes];

    size_t numBodies = 0;
    cpBody bodies[kMaxShapes];
};

static const size_t kNumSubSteps = 10;
const cpFloat kStepTimeS = 1./60.;          // step time, in seconds


#define SPACE           (sim->physicsSpace)

#define BODY(IDX)       (&sim->bodies[(IDX)])
#define CIRCLE(IDX)     (&(sim->circles[(IDX)]))
#define BOX(IDX)        (&(sim->boxes[(IDX)]))

#define BODY_ALLOC()    (BODY(sim->numBodies++))
#define CIRCLE_ALLOC()  (CIRCLE(sim->numCircles++))
#define BOX_ALLOC()     (BOX(sim->numBoxes++))

#define CIRCLE_IDX(VAL) ((((uintptr_t)(VAL)) - ((uintptr_t)(&sim->circles[0]))) / sizeof(sim->circles[0]))
#define BOX_IDX(VAL)    ((((uintptr_t)(VAL)) - ((uintptr_t)(&sim->boxes[0]))) / sizeof(sim->boxes[0]))


void FSTUFF_SimulationInit(FSTUFF_Simulation * sim, void * buffer, size_t bufSize, id<MTLDevice> gpuDevice)
{
    //
    // GPU init
    //
    sim->circleFilled.debugName = "FSTUFF_CircleFilled";
    sim->circleFilled.type = FSTUFF_ShapeCircle;
    sim->circleFilled.appearance = FSTUFF_ShapeAppearanceFilled;
    sim->circleFilled.circle.numParts = kNumCircleParts;
    FSTUFF_ShapeInit(&(sim->circleFilled), buffer, bufSize, gpuDevice);

    sim->circleEdged.debugName = "FSTUFF_CircleEdged";
    sim->circleEdged.type = FSTUFF_ShapeCircle;
    sim->circleEdged.appearance = FSTUFF_ShapeAppearanceEdged;
    sim->circleEdged.circle.numParts = kNumCircleParts;
    FSTUFF_ShapeInit(&(sim->circleEdged), buffer, bufSize, gpuDevice);

    sim->boxFilled.debugName = "FSTUFF_BoxEdged";
    sim->boxFilled.type = FSTUFF_ShapeBox;
    sim->boxFilled.appearance = FSTUFF_ShapeAppearanceFilled;
    FSTUFF_ShapeInit(&(sim->boxFilled), buffer, bufSize, gpuDevice);
    
    sim->boxEdged.debugName = "FSTUFF_BoxEdged";
    sim->boxEdged.type = FSTUFF_ShapeBox;
    sim->boxEdged.appearance = FSTUFF_ShapeAppearanceEdged;
    FSTUFF_ShapeInit(&(sim->boxEdged), buffer, bufSize, gpuDevice);
    
    //
    // Physics-world init
    //
    memset(&sim->_physicsSpaceStorage, 0, sizeof(sim->_physicsSpaceStorage));
    cpSpaceInit(sim->physicsSpace);
    cpSpaceSetIterations(sim->physicsSpace, 100);
    cpSpaceSetGravity(sim->physicsSpace, cpv(0, -98));
    // TODO: try resizing cpSpace hashes
    
    cpBody * body;
    cpShape * shape;
    
    body = cpBodyInit(BODY_ALLOC(), 0, 0);
    cpSpaceAddBody(SPACE, body);
    cpBodySetPosition(body, cpv(40, 80));
    shape = (cpShape*)cpCircleShapeInit(CIRCLE_ALLOC(), body, 4, cpvzero);
    cpSpaceAddShape(sim->physicsSpace, shape);
    cpShapeSetDensity(shape, 10);
    cpShapeSetElasticity(shape, 0.8);
    sim->circleColors[CIRCLE_IDX(shape)] = FSTUFF_Color(0xff0000);

    body = cpBodyInit(BODY_ALLOC(), 0, 0);
    cpBodySetType(body, CP_BODY_TYPE_STATIC);
    cpSpaceAddBody(SPACE, body);
    cpBodySetPosition(body, cpv(35, 20));
    shape = (cpShape*)cpCircleShapeInit(CIRCLE_ALLOC(), body, 10, cpvzero);
    cpSpaceAddShape(SPACE, shape);
    cpShapeSetElasticity(shape, 0.8);
    sim->circleColors[CIRCLE_IDX(shape)] = FSTUFF_Color(0x00ff00);

//    body = cpBodyInit(BODY_ALLOC(), 0, 0);
//    cpBodySetType(body, CP_BODY_TYPE_STATIC);
//    cpSpaceAddBody(SPACE, body);
//    cpBodySetPosition(body, cpv(35, 20));
//    //shape = (cpShape*)cpCircleShapeInit(CIRCLE_ALLOC(), body, 10, cpvzero);
//    static const cpFloat wallThickness = 10.0;
//    static const cpFloat wallLeft = -wallThickness / 2.;
//    //static const cpFloat wallTop = -sim->viewSizeMM.y;
//    static const cpFloat wallBottom = sim->viewSizeMM.y + (wallThickness / 2.);
//    static const cpFloat wallRight = sim->viewSizeMM.x + (wallThickness / 2.);
//    shape = (cpShape*)cpSegmentShapeInit(SEGMENT_ALLOC(), body, cpv(wallLeft,wallBottom), cpv(wallRight,wallBottom), wallThickness);
//    cpSpaceAddShape(SPACE, shape);
//    cpShapeSetElasticity(shape, 0.8);

    body = cpBodyInit(BODY_ALLOC(), 0, 0);
    cpBodySetType(body, CP_BODY_TYPE_STATIC);
    cpSpaceAddBody(SPACE, body);
    cpBodySetPosition(body, cpv(30, 5.));
    cpBodySetAngle(body, M_PI / 6.);
    shape = (cpShape*)cpSegmentShapeInit(BOX_ALLOC(), body, cpv(0.,0.), cpv(100.,0.), 5.);
    cpSpaceAddShape(SPACE, shape);
    cpShapeSetElasticity(shape, 0.8);
    sim->boxColors[BOX_IDX(shape)] = FSTUFF_Color(0x0000ff);
}

void FSTUFF_SimulationShutdown(FSTUFF_Simulation * sim)
{
    for (size_t i = 0; i < sim->numCircles; ++i) {
        cpShapeDestroy((cpShape*)CIRCLE(i));
    }
    for (size_t i = 0; i < sim->numBoxes; ++i) {
        cpShapeDestroy((cpShape*)BOX(i));
    }
    for (size_t i = 0; i < sim->numBodies; ++i) {
        cpBodyDestroy(BODY(i));
    }
    cpSpaceDestroy(sim->physicsSpace);
}

void FSTUFF_SimulationViewChanged(FSTUFF_Simulation * sim, float widthMM, float heightMM)
{
    sim->viewSizeMM = {widthMM, heightMM};
    
    matrix_float4x4 t = matrix_from_translation(-1, -1, 0);
    matrix_float4x4 s = matrix_from_scaling(2.0f / widthMM, 2.0f / heightMM, 1);
    sim->projectionMatrix = matrix_multiply(t, s);
}

void FSTUFF_SimulationUpdate(FSTUFF_Simulation * sim, FSTUFF_GPUData * gpuData)
{
    // Compute current time
    cpFloat nowS;           // current time, in seconds since UNIX epoch
    struct timeval nowSys;  // used to get current time from OS
    gettimeofday(&nowSys, NULL);
    nowS = (cpFloat)nowSys.tv_sec + ((cpFloat)nowSys.tv_usec / 1000000.);
    
    // Initialize simulation time vars, on first tick
    if (sim->lastUpdateUTCTimeS == 0.) {
        sim->lastUpdateUTCTimeS = nowS;
    }
    
    // Update physics
    const cpFloat kSubstepTimeS = kStepTimeS / ((cpFloat)kNumSubSteps);
    while ((sim->lastUpdateUTCTimeS + kStepTimeS) <= nowS) {
        for (size_t i = 0; i < kNumSubSteps; ++i) {
            sim->lastUpdateUTCTimeS += kSubstepTimeS;
            cpSpaceStep(SPACE, kSubstepTimeS);
        }
    }

    // Copy simulation/game data to GPU-accessible buffers
    gpuData->globals.projection_matrix = sim->projectionMatrix;
    
    for (size_t i = 0; i < sim->numCircles; ++i) {
        cpFloat shapeRadius = cpCircleShapeGetRadius((cpShape*)CIRCLE(i));
        cpBody * body = cpShapeGetBody((cpShape*)CIRCLE(i));
        cpVect bodyCenter = cpBodyGetPosition(body);
        gpuData->circles[i].model_matrix = matrix_multiply(
            matrix_from_translation(bodyCenter.x, bodyCenter.y, 0),
            matrix_from_scaling(shapeRadius, shapeRadius, 1)
        );
        gpuData->circles[i].color = sim->circleColors[i];
    }

    sim->numBoxes = 1;
    for (size_t i = 0; i < sim->numBoxes; ++i) {
        cpVect a = cpSegmentShapeGetA((cpShape*)BOX(i));
        cpVect b = cpSegmentShapeGetB((cpShape*)BOX(i));
        cpFloat radius = cpSegmentShapeGetRadius((cpShape*)BOX(i));
        cpBody * body = cpShapeGetBody((cpShape*)BOX(i));
        cpVect bodyCenter = cpBodyGetPosition(body);
        cpFloat bodyAngle = cpBodyGetAngle(body);
        matrix_float4x4 m = matrix_identity_float4x4;
        m = matrix_multiply(m, matrix_from_translation(bodyCenter.x, bodyCenter.y, 0.));
        m = matrix_multiply(m, matrix_from_rotation(bodyAngle, 0., 0., 1.));
        m = matrix_multiply(m, matrix_from_translation(((b.x-a.x)/2.f)+a.x, ((b.y-a.y)/2.f)+a.y, 0.));
        m = matrix_multiply(m, matrix_from_scaling(cpvlength(b-a), radius*2., 1.));
        gpuData->boxes[i].model_matrix = m;
        gpuData->boxes[i].color = sim->boxColors[i];
    }


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
}

void FSTUFF_SimulationRender(FSTUFF_Simulation * sim,
                             id <MTLRenderCommandEncoder> gpuRenderer,
                             id <MTLBuffer> gpuAppData)
{
    FSTUFF_RenderShapes(&sim->circleFilled, sim->numCircles,    gpuRenderer, gpuAppData, 0.35f);
    FSTUFF_RenderShapes(&sim->circleEdged,  sim->numCircles,    gpuRenderer, gpuAppData, 1.0f);
    FSTUFF_RenderShapes(&sim->boxFilled,    sim->numBoxes,      gpuRenderer, gpuAppData, 0.35f);
    FSTUFF_RenderShapes(&sim->boxEdged,     sim->numBoxes,      gpuRenderer, gpuAppData, 1.0f);
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
        _gpuConstants[i] = [_device newBufferWithLength:kMaxBytesPerFrame options:0];
        _gpuConstants[i].label = [NSString stringWithFormat:@"FSTUFF_ConstantBuffer%i", i];
    }
}

- (void)_update
{
    const uint8_t * constants = (uint8_t *) [_gpuConstants[_constantDataBufferIndex] contents];
    FSTUFF_GPUData * gpuData = (FSTUFF_GPUData *) constants;
    FSTUFF_SimulationUpdate(&_sim, gpuData);
}

void FSTUFF_RenderShapes(FSTUFF_ShapeTemplate * shape,
                         size_t count,
                         id <MTLRenderCommandEncoder> gpuRenderer,
                         id <MTLBuffer> gpuData,    // laid out as a 'FSTUFF_GPUData' struct
                         float alpha)
{
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
    [gpuRenderer setVertexBuffer:shape->gpuVertexBuffer offset:0 atIndex:0];                    // 'position[<vertex id>]'
    [gpuRenderer setVertexBuffer:gpuData offset:offsetof(FSTUFF_GPUData, globals) atIndex:1];   // 'gpuGlobals'
    [gpuRenderer setVertexBuffer:gpuData offset:shapesOffsetInGpuData atIndex:2];               // 'gpuShapes[<instance id>]'
    [gpuRenderer setVertexBytes:&alpha length:sizeof(alpha) atIndex:3];                         // 'alpha'
    
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
        FSTUFF_SimulationRender(&_sim, gpuRenderer, _gpuConstants[_constantDataBufferIndex]);
        
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
