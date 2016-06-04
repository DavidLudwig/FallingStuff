//
//  FSTUFF_Simulation.cpp
//  FallingStuff
//
//  Created by David Ludwig on 5/30/16.
//  Copyright © 2016 David Ludwig. All rights reserved.
//


#include "FSTUFF_GPU.h"
#import <simd/simd.h>
#import "SharedStructures.h"
#include <sys/time.h>   // for gettimeofday()
#include <random>
#include "FSTUFF_Colors.h"

#include "FSTUFF_Simulation.h"

static matrix_float4x4 matrix_from_translation(float x, float y, float z);
static matrix_float4x4 matrix_from_scaling(float x, float y, float z);
static matrix_float4x4 matrix_from_rotation(float radians, float x, float y, float z);



static const unsigned kNumCircleParts = 64; //32;

#define FSTUFF_countof(arr) (sizeof(arr) / sizeof(arr[0]))

#define RAD_IDX(I) (((float)I) * kRadianStep)
#define COS_IDX(I) ((float)cos(RAD_IDX(I)))
#define SIN_IDX(I) ((float)sin(RAD_IDX(I)))

void FSTUFF_MakeCircleFilledTriangles(vector_float4 * vertices,
                                      int maxVertices,
                                      int * numVertices,
                                      int numPartsToGenerate,
                                      float radius,
                                      float offsetX,
                                      float offsetY)
{
//    // TODO: check the size of the vertex buffer!
//    static const int kVertsPerPart = 3;
//    const float kRadianStep = ((((float)M_PI) * 2.0f) / (float)numPartsToGenerate);
//    *numVertices = numPartsToGenerate * kVertsPerPart;
//    for (unsigned i = 0; i < numPartsToGenerate; ++i) {
//        vertices[(i * kVertsPerPart) + 0] = {           0,            0, 0, 1};
//        vertices[(i * kVertsPerPart) + 1] = {COS_IDX( i ), SIN_IDX( i ), 0, 1};
//        vertices[(i * kVertsPerPart) + 2] = {COS_IDX(i+1), SIN_IDX(i+1), 0, 1};
//    }

//    numPartsToGenerate = 8;

    const vector_float4 * verticesOrig = vertices;
    const int n = numPartsToGenerate / 2;
    float rad = 0.;
    const float radStep = M_PI/(float)n;

    // start
    *vertices++ = { (1.f*radius) + offsetX,                 ( 0.f*radius) + offsetY,                  0,  1};
    *vertices++ = { (cosf(rad + radStep)*radius) + offsetX, (-sinf(rad + radStep)*radius) + offsetY,  0,  1};
    *vertices++ = { (cosf(rad + radStep)*radius) + offsetX, ( sinf(rad + radStep)*radius) + offsetY,  0,  1};
    rad += radStep;
    
    // mid
    for (int i = 1; i <= (n-2); ++i) {
        *vertices++ = { (cosf(rad)*radius) + offsetX,         ( sinf(rad)*radius) + offsetY,            0,  1};
        *vertices++ = { (cosf(rad)*radius) + offsetX,         (-sinf(rad)*radius) + offsetY,            0,  1};
        *vertices++ = { (cosf(rad+radStep)*radius) + offsetX, (-sinf(rad+radStep)*radius) + offsetY,    0,  1};
        *vertices++ = { (cosf(rad+radStep)*radius) + offsetX, (-sinf(rad+radStep)*radius) + offsetY,    0,  1};
        *vertices++ = { (cosf(rad+radStep)*radius) + offsetX, ( sinf(rad+radStep)*radius) + offsetY,    0,  1};
        *vertices++ = { (cosf(rad)*radius) + offsetX,         ( sinf(rad)*radius) + offsetY,            0,  1};

        rad += radStep;
    }
    
    // end
    *vertices++ = { (cosf(rad)*radius) + offsetX,           ( sinf(rad)*radius) + offsetY,            0,  1};
    *vertices++ = { (cosf(rad)*radius) + offsetX,           (-sinf(rad)*radius) + offsetY,            0,  1};
    *vertices++ = { (-1.f*radius) + offsetX,                ( 0.f*radius) + offsetY,                  0,  1};

    *numVertices = (int) (((intptr_t)vertices - (intptr_t)verticesOrig) / sizeof(vertices[0]));
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

void FSTUFF_ShapeInit(FSTUFF_ShapeTemplate * shape, void * device)
{
    // Generate vertices in CPU-accessible memory
    vector_float4 vertices[2048];
    const size_t maxElements = FSTUFF_countof(vertices);
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
            FSTUFF_MakeCircleFilledTriangles(vertices, maxElements, &shape->numVertices, shape->circle.numParts, 1.f, 0.f, 0.f);
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
        shape->gpuVertexBuffer = FSTUFF_VertexBufferNew(device,
                                                        vertices,
                                                        shape->numVertices * sizeof(vector_float4));
    } else {
        shape->gpuVertexBuffer = NULL;
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

cpFloat FSTUFF_RandRangeF(std::mt19937 & rng, cpFloat a, cpFloat b)
{
    std::uniform_real_distribution<cpFloat> distribution(a, b);
    return distribution(rng);
}

int FSTUFF_RandRangeI(std::mt19937 & rng, int a, int b)
{
    std::uniform_int_distribution<int> distribution(a, b);
    return distribution(rng);
}

void FSTUFF_RenderShapes(FSTUFF_ShapeTemplate * shape,
                         size_t offset,
                         size_t count,
                         void * gpuDevice,
                         void * gpuRenderer,
                         void * gpuAppData,
                         float alpha);


#pragma mark - Simulation

static const size_t kNumSubSteps = 10;
const cpFloat kStepTimeS = 1./60.;          // step time, in seconds



#define SPACE           (sim->world.physicsSpace)

#define BODY(IDX)       (&sim->world.bodies[(IDX)])
#define CIRCLE(IDX)     (&(sim->world.circles[(IDX)]))
#define BOX(IDX)        (&(sim->world.boxes[(IDX)]))

#define BODY_ALLOC()    (BODY(sim->world.numBodies++))
#define CIRCLE_ALLOC()  (CIRCLE(sim->world.numCircles++))
#define BOX_ALLOC()     (BOX(sim->world.numBoxes++))

#define CIRCLE_IDX(VAL) ((((uintptr_t)(VAL)) - ((uintptr_t)(&sim->world.circles[0]))) / sizeof(sim->world.circles[0]))
#define BOX_IDX(VAL)    ((((uintptr_t)(VAL)) - ((uintptr_t)(&sim->world.boxes[0]))) / sizeof(sim->world.boxes[0]))


void FSTUFF_SimulationInitGPU(FSTUFF_Simulation * sim, void * gpuDevice)
{
    //
    // GPU init
    //
    sim->circleFilled.debugName = "FSTUFF_CircleFilled";
    sim->circleFilled.type = FSTUFF_ShapeCircle;
    sim->circleFilled.appearance = FSTUFF_ShapeAppearanceFilled;
    sim->circleFilled.circle.numParts = kNumCircleParts;
    FSTUFF_ShapeInit(&(sim->circleFilled), gpuDevice);

    sim->circleDots.debugName = "FSTUFF_CircleDots";
    sim->circleDots.type = FSTUFF_ShapeCircle;
    sim->circleDots.appearance = FSTUFF_ShapeAppearanceFilled;
    sim->circleDots.circle.numParts = kNumCircleParts;
    sim->circleDots.primitiveType = FSTUFF_PrimitiveTriangles;
    {
        vector_float4 vertices[2048];
        const int numCirclePartsForDot = 6;
        const float dotRadius = 0.08f;  // size of dot: 0 to 1; 0 is no-size, 1 is as big as containing-circle
        const float dotDistance = 0.7f; // from 0 to 1
        int tmpVertexCount;
        sim->circleDots.numVertices = 0;
        for (int i = 0, n = 6; i < n; ++i) {
            const float rad = float(i) * ((M_PI * 2.f) / float(n));
            FSTUFF_MakeCircleFilledTriangles(
                &vertices[sim->circleDots.numVertices],
                0,
                &tmpVertexCount,
                numCirclePartsForDot,
                dotRadius,
                cosf(rad) * dotDistance,
                sinf(rad) * dotDistance
            );
            sim->circleDots.numVertices += tmpVertexCount;
        }
        sim->circleDots.gpuVertexBuffer = FSTUFF_VertexBufferNew(gpuDevice, vertices, (sim->circleDots.numVertices * sizeof(vector_float4)));
    }

    sim->circleEdged.debugName = "FSTUFF_CircleEdged";
    sim->circleEdged.type = FSTUFF_ShapeCircle;
    sim->circleEdged.appearance = FSTUFF_ShapeAppearanceEdged;
    sim->circleEdged.circle.numParts = kNumCircleParts;
    FSTUFF_ShapeInit(&(sim->circleEdged), gpuDevice);

    sim->boxFilled.debugName = "FSTUFF_BoxEdged";
    sim->boxFilled.type = FSTUFF_ShapeBox;
    sim->boxFilled.appearance = FSTUFF_ShapeAppearanceFilled;
    FSTUFF_ShapeInit(&(sim->boxFilled), gpuDevice);
    
    sim->boxEdged.debugName = "FSTUFF_BoxEdged";
    sim->boxEdged.type = FSTUFF_ShapeBox;
    sim->boxEdged.appearance = FSTUFF_ShapeAppearanceEdged;
    FSTUFF_ShapeInit(&(sim->boxEdged), gpuDevice);
}

void FSTUFF_SimulationInitWorld(FSTUFF_Simulation * sim)
{
    //
    // Physics-world init
    //
    memset(&sim->world, 0, sizeof(FSTUFF_Simulation::World));
    sim->world.physicsSpace = (cpSpace *) &(sim->world._physicsSpaceStorage);
    cpSpaceInit(sim->world.physicsSpace);
    cpSpaceSetIterations(sim->world.physicsSpace, 100);
    cpSpaceSetGravity(sim->world.physicsSpace, cpv(0, -98));
    // TODO: try resizing cpSpace hashes

    cpBody * body;
    cpShape * shape;
    
    
    //
    // Walls
    //

    body = cpBodyInit(BODY_ALLOC(), 0, 0);
    cpBodySetType(body, CP_BODY_TYPE_STATIC);
    cpSpaceAddBody(SPACE, body);
    cpBodySetPosition(body, cpv(0, 0));
    static const cpFloat wallThickness = 5.0;
    static const cpFloat wallLeft   = -wallThickness / 2.;
    static const cpFloat wallRight  = sim->viewSizeMM.x + (wallThickness / 2.);
    static const cpFloat wallBottom = -wallThickness / 2.;
    static const cpFloat wallTop    = sim->viewSizeMM.y * 2.;   // use a high ceiling, to make sure off-screen falling things don't go over walls
    
    // Bottom
    shape = (cpShape*)cpSegmentShapeInit(BOX_ALLOC(), body, cpv(wallLeft,wallBottom), cpv(wallRight,wallBottom), wallThickness/2.);
    cpSpaceAddShape(SPACE, shape);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    sim->world.boxColors[BOX_IDX(shape)] = FSTUFF_Color(0x000000, 0x00);
    // Left
    shape = (cpShape*)cpSegmentShapeInit(BOX_ALLOC(), body, cpv(wallLeft,wallBottom), cpv(wallLeft,wallTop), wallThickness/2.);
    cpSpaceAddShape(SPACE, shape);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    sim->world.boxColors[BOX_IDX(shape)] = FSTUFF_Color(0x000000, 0x00);
    // Right
    shape = (cpShape*)cpSegmentShapeInit(BOX_ALLOC(), body, cpv(wallRight,wallBottom), cpv(wallRight,wallTop), wallThickness/2.);
    cpSpaceAddShape(SPACE, shape);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    sim->world.boxColors[BOX_IDX(shape)] = FSTUFF_Color(0x000000, 0x00);


    //
    // Pegs
    //
    using namespace FSTUFF_Colors;
    const int pegColors[] = {
        Red,
        Red,
        Lime,
        Lime,
        Blue,
        Blue,
        Yellow,
        Cyan,
    };
    const int numPegs = round((sim->viewSizeMM.x * sim->viewSizeMM.y) * 0.0005);
    const cpFloat kPegScaleCircle = 2.5;
    const cpFloat kPegScaleBox = 4.;
    std::random_device rd;
    std::mt19937 rng(rd());
    cpFloat cx, cy, radius, w, h, angleRad;
    int pegColorIndex;
    for (int i = 0; i < numPegs; ++i) {
        switch (rand() % 2) {
            case 0:
            {
                cx = FSTUFF_RandRangeF(rng, 0., sim->viewSizeMM.x);
                cy = FSTUFF_RandRangeF(rng, 0., sim->viewSizeMM.y);
                radius = kPegScaleCircle * FSTUFF_RandRangeF(rng, 6., 10.);
                pegColorIndex = FSTUFF_RandRangeI(rng, 0, FSTUFF_countof(pegColors)-1);

                body = cpBodyInit(BODY_ALLOC(), 0, 0);
                cpBodySetType(body, CP_BODY_TYPE_STATIC);
                cpSpaceAddBody(SPACE, body);
                cpBodySetPosition(body, cpv(cx, cy));
                shape = (cpShape*)cpCircleShapeInit(CIRCLE_ALLOC(), body, radius, cpvzero);
                ++sim->world.numPegs;
                cpSpaceAddShape(SPACE, shape);
                cpShapeSetElasticity(shape, 0.8);
                cpShapeSetFriction(shape, 1);
                sim->world.circleColors[CIRCLE_IDX(shape)] = FSTUFF_Color(pegColors[pegColorIndex]);
            } break;
            
            case 1:
            {
                cx = FSTUFF_RandRangeF(rng, 0., sim->viewSizeMM.x);
                cy = FSTUFF_RandRangeF(rng, 0., sim->viewSizeMM.y);
                w = kPegScaleBox * FSTUFF_RandRangeF(rng, 6., 14.);
                h = kPegScaleBox * FSTUFF_RandRangeF(rng, 1., 2.);
                angleRad = FSTUFF_RandRangeF(rng, 0., M_PI);
                pegColorIndex = FSTUFF_RandRangeI(rng, 0, FSTUFF_countof(pegColors)-1);
            
                body = cpBodyInit(BODY_ALLOC(), 0, 0);
                cpBodySetType(body, CP_BODY_TYPE_STATIC);
                cpSpaceAddBody(SPACE, body);
                cpBodySetPosition(body, cpv(cx, cy));
                cpBodySetAngle(body, angleRad);
                shape = (cpShape*)cpSegmentShapeInit(BOX_ALLOC(), body, cpv(-w/2.,0.), cpv(w/2.,0.), h/2.);
                cpSpaceAddShape(SPACE, shape);
                cpShapeSetElasticity(shape, 0.8);
                sim->world.boxColors[BOX_IDX(shape)] = FSTUFF_Color(pegColors[pegColorIndex]);
            } break;
        }
    }
    
    
    //
    // Marbles
    //

    body = cpBodyInit(BODY_ALLOC(), 0, 0);
    cpSpaceAddBody(SPACE, body);
    cpBodySetPosition(body, cpv(40, sim->viewSizeMM.y * 1.2));
    shape = (cpShape*)cpCircleShapeInit(CIRCLE_ALLOC(), body, 4, cpvzero);
    cpSpaceAddShape(sim->world.physicsSpace, shape);
    cpShapeSetDensity(shape, 10);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    sim->world.circleColors[CIRCLE_IDX(shape)] = FSTUFF_Color(White);

    body = cpBodyInit(BODY_ALLOC(), 0, 0);
    cpSpaceAddBody(SPACE, body);
    cpBodySetPosition(body, cpv(80, sim->viewSizeMM.y * 1.1));
    shape = (cpShape*)cpCircleShapeInit(CIRCLE_ALLOC(), body, 4, cpvzero);
    cpSpaceAddShape(sim->world.physicsSpace, shape);
    cpShapeSetDensity(shape, 10);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    sim->world.circleColors[CIRCLE_IDX(shape)] = FSTUFF_Color(White);
}

void FSTUFF_SimulationInit(FSTUFF_Simulation * sim, void * gpuDevice)
{
    FSTUFF_SimulationInitGPU(sim, gpuDevice);
    FSTUFF_SimulationInitWorld(sim);
}

void FSTUFF_SimulationShutdownWorld(FSTUFF_Simulation * sim);

void FSTUFF_SimulationEvent(FSTUFF_Simulation * sim, FSTUFF_Event * event)
{
    switch (event->type) {
        case FSTUFF_EventKeyDown: {
            switch (std::toupper(event->key.utf8[0])) {
                case 'R': {
                    FSTUFF_SimulationShutdownWorld(sim);
                    FSTUFF_SimulationInitWorld(sim);
                } break;
            }
        } break;
    }
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
    
    for (size_t i = 0; i < sim->world.numCircles; ++i) {
        cpFloat shapeRadius = cpCircleShapeGetRadius((cpShape*)CIRCLE(i));
        cpBody * body = cpShapeGetBody((cpShape*)CIRCLE(i));
        cpVect bodyCenter = cpBodyGetPosition(body);
        cpFloat bodyAngle = cpBodyGetAngle(body);
        matrix_float4x4 m = matrix_identity_float4x4;
        m = matrix_multiply(m, matrix_from_translation(bodyCenter.x, bodyCenter.y, 0.));
        m = matrix_multiply(m, matrix_from_rotation(bodyAngle, 0., 0., 1.));
        m = matrix_multiply(m, matrix_from_scaling(shapeRadius, shapeRadius, 1));
        gpuData->circles[i].model_matrix = m;
        gpuData->circles[i].color = sim->world.circleColors[i];
    }

    for (size_t i = 0; i < sim->world.numBoxes; ++i) {
        cpVect a = cpSegmentShapeGetA((cpShape*)BOX(i));
        cpVect b = cpSegmentShapeGetB((cpShape*)BOX(i));
        cpVect center = cpvlerp(a, b, 0.5);
        cpFloat radius = cpSegmentShapeGetRadius((cpShape*)BOX(i));
        cpBody * body = cpShapeGetBody((cpShape*)BOX(i));
        cpVect bodyCenter = cpBodyGetPosition(body);
        cpFloat bodyAngle = cpBodyGetAngle(body);
        matrix_float4x4 m = matrix_identity_float4x4;
        m = matrix_multiply(m, matrix_from_translation(bodyCenter.x, bodyCenter.y, 0.));
        m = matrix_multiply(m, matrix_from_rotation(bodyAngle, 0., 0., 1.));
        m = matrix_multiply(m, matrix_from_translation(center.x, center.y, 0.));
        m = matrix_multiply(m, matrix_from_rotation(cpvtoangle(b-a), 0., 0., 1.));
        m = matrix_multiply(m, matrix_from_scaling(cpvlength(b-a), radius*2., 1.));    // TODO: check to see if this is correct
        gpuData->boxes[i].model_matrix = m;
        gpuData->boxes[i].color = sim->world.boxColors[i];
    }


/*
	self.unlit_peg_fill_alpha_min = 0.25
	self.unlit_peg_fill_alpha_max = 0.45

 pb.fill_alpha = rand_in_range(self.unlit_peg_fill_alpha_min, self.unlit_peg_fill_alpha_max)
*/
}

void FSTUFF_SimulationRender(FSTUFF_Simulation * sim,
                             void * gpuDevice,
                             void * gpuRenderer,
                             void * gpuAppData)
{
    FSTUFF_RenderShapes(&sim->circleFilled, 0,                  sim->world.numCircles,                      gpuDevice, gpuRenderer, gpuAppData, 0.35f);
    FSTUFF_RenderShapes(&sim->circleDots,   sim->world.numPegs, sim->world.numCircles - sim->world.numPegs, gpuDevice, gpuRenderer, gpuAppData, 1.0f);
    FSTUFF_RenderShapes(&sim->circleEdged,  0,                  sim->world.numCircles,                      gpuDevice, gpuRenderer, gpuAppData, 1.0f);
    FSTUFF_RenderShapes(&sim->boxFilled,    0,                  sim->world.numBoxes,                        gpuDevice, gpuRenderer, gpuAppData, 0.35f);
    FSTUFF_RenderShapes(&sim->boxEdged,     0,                  sim->world.numBoxes,                        gpuDevice, gpuRenderer, gpuAppData, 1.0f);
}

void FSTUFF_SimulationViewChanged(FSTUFF_Simulation * sim, float widthMM, float heightMM)
{
    sim->viewSizeMM = {widthMM, heightMM};
//    NSLog(@"view size: %f, %f", widthMM, heightMM);
    
    matrix_float4x4 t = matrix_from_translation(-1, -1, 0);
    matrix_float4x4 s = matrix_from_scaling(2.0f / widthMM, 2.0f / heightMM, 1);
    sim->projectionMatrix = matrix_multiply(t, s);
}

void FSTUFF_SimulationShutdownWorld(FSTUFF_Simulation * sim)
{
    for (size_t i = 0; i < sim->world.numCircles; ++i) {
        cpShapeDestroy((cpShape*)CIRCLE(i));
    }
    for (size_t i = 0; i < sim->world.numBoxes; ++i) {
        cpShapeDestroy((cpShape*)BOX(i));
    }
    for (size_t i = 0; i < sim->world.numBodies; ++i) {
        cpBodyDestroy(BODY(i));
    }
    cpSpaceDestroy(sim->world.physicsSpace);
}

static void FSTUFF_SimulationShutdownGPU(FSTUFF_Simulation * sim)
{
    if (sim->circleDots.gpuVertexBuffer) {
        FSTUFF_VertexBufferDestroy(sim->circleDots.gpuVertexBuffer);
    }
    if (sim->circleEdged.gpuVertexBuffer) {
        FSTUFF_VertexBufferDestroy(sim->circleEdged.gpuVertexBuffer);
    }
    if (sim->circleFilled.gpuVertexBuffer) {
        FSTUFF_VertexBufferDestroy(sim->circleFilled.gpuVertexBuffer);
    }
    if (sim->boxEdged.gpuVertexBuffer) {
        FSTUFF_VertexBufferDestroy(sim->boxEdged.gpuVertexBuffer);
    }
    if (sim->boxFilled.gpuVertexBuffer) {
        FSTUFF_VertexBufferDestroy(sim->boxFilled.gpuVertexBuffer);
    }
}

void FSTUFF_SimulationShutdown(FSTUFF_Simulation * sim)
{
    FSTUFF_SimulationShutdownWorld(sim);
    FSTUFF_SimulationShutdownGPU(sim);
}


#pragma mark Utilities

matrix_float4x4 matrix_from_translation(float x, float y, float z)
{
    matrix_float4x4 m = matrix_identity_float4x4;
    m.columns[3] = (vector_float4) { x, y, z, 1.0 };
    return m;
}

matrix_float4x4 matrix_from_scaling(float x, float y, float z)
{
    matrix_float4x4 m = matrix_identity_float4x4;
    m.columns[0][0] = x;
    m.columns[1][1] = y;
    m.columns[2][2] = z;
    m.columns[3][3] = 1.0f;
    return m;
}

matrix_float4x4 matrix_from_rotation(float radians, float x, float y, float z)
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
