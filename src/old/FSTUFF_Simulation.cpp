//
//  FSTUFF_Simulation.cpp
//  FallingStuff
//
//  Created by David Ludwig on 5/30/16.
//  Copyright © 2018 David Ludwig. All rights reserved.
//


#import <simd/simd.h>
#import "FSTUFF_AppleMetalStructs.h"
#include <sys/time.h>   // for gettimeofday()
#include <random>
#include "FSTUFF.h"

#define GB_MATH_IMPLEMENTATION
#include "gb_math.h"


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

void FSTUFF_ShapeInit(FSTUFF_Shape * shape, void * device)
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

void FSTUFF_RenderShapes(FSTUFF_Renderer * renderer,
                         FSTUFF_Shape * shape,
                         size_t offset,
                         size_t count,
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


void FSTUFF_InitGPUShapes(FSTUFF_Simulation * sim, void * gpuDevice)
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

void FSTUFF_InitWorld(FSTUFF_Simulation * sim)
{
    //
    // Physics-world init
    //
    memset(&sim->world, 0, sizeof(FSTUFF_Simulation::World));
    //sim->world.physicsSpace = (cpSpace *) &(sim->world._physicsSpaceStorage);
    //cpSpaceInit(sim->world.physicsSpace);
    sim->world.physicsSpace = cpSpaceNew();
    
    cpSpaceSetIterations(sim->world.physicsSpace, 2);
    cpSpaceSetGravity(sim->world.physicsSpace, sim->gravity);
    // TODO: try resizing cpSpace hashes
    //cpSpaceUseSpatialHash(sim->world.physicsSpace, 2, 10);

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
    cpFloat cx, cy, radius, w, h, angleRad;
    int pegColorIndex;
    for (int i = 0; i < numPegs; ++i) {
        switch (rand() % 2) {
            case 0:
            {
                cx = FSTUFF_RandRangeF(sim->rng, 0., sim->viewSizeMM.x);
                cy = FSTUFF_RandRangeF(sim->rng, 0., sim->viewSizeMM.y);
                radius = kPegScaleCircle * FSTUFF_RandRangeF(sim->rng, 6., 10.);
                pegColorIndex = FSTUFF_RandRangeI(sim->rng, 0, FSTUFF_countof(pegColors)-1);

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
                cx = FSTUFF_RandRangeF(sim->rng, 0., sim->viewSizeMM.x);
                cy = FSTUFF_RandRangeF(sim->rng, 0., sim->viewSizeMM.y);
                w = kPegScaleBox * FSTUFF_RandRangeF(sim->rng, 6., 14.);
                h = kPegScaleBox * FSTUFF_RandRangeF(sim->rng, 1., 2.);
                angleRad = FSTUFF_RandRangeF(sim->rng, 0., M_PI);
                pegColorIndex = FSTUFF_RandRangeI(sim->rng, 0, FSTUFF_countof(pegColors)-1);
            
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
    
//    for (int i = 0; i < 1500; i++) {
//        FSTUFF_AddMarble(sim);
//    }
}

void FSTUFF_Init(FSTUFF_Simulation * sim) //, void * gpuDevice, void * nativeView)
{
    FSTUFF_Log("%s, sim:%p, state:%d, gpuDevice:%p, nativeView:%p\n",
        __FUNCTION__, sim, sim->state, sim->gpuDevice, sim->nativeView);
    
    // Don't re-initialize simulations that are already alive
    if (sim->state != FSTUFF_DEAD) {
        return;
    }

    // Preserve OS-native resource handles, within 'sim'
    void * gpuDevice = sim->gpuDevice;
    void * nativeView = sim->nativeView;

    // Reset all variables in 'sim'
    sim = new (sim) FSTUFF_Simulation;

    // Mark simulation as alive
    sim->state = FSTUFF_ALIVE;

    // Restore OS-native resource handles, to 'sim'
    sim->gpuDevice = gpuDevice;
    sim->nativeView = nativeView;

    // Initialize 'sim'
    float widthMM = 0.f;
    float heightMM = 0.f;
    FSTUFF_GetViewSizeMM(nativeView, &widthMM, &heightMM);
    FSTUFF_ViewChanged(sim, widthMM, heightMM);
    FSTUFF_InitGPUShapes(sim, gpuDevice);
    FSTUFF_InitWorld(sim);
}

void FSTUFF_ShutdownWorld(FSTUFF_Simulation * sim);

FSTUFF_Event FSTUFF_NewKeyEvent(FSTUFF_EventType eventType, const char * utf8Char)
{
    FSTUFF_Event event;
    memset(&event, 0, sizeof(event));
    event.type = eventType;
    const size_t srcSize = strlen(utf8Char) + 1;
    const size_t copySize = std::min(sizeof(event.data.key.utf8), srcSize);
    strlcpy(const_cast<char *>(event.data.key.utf8), utf8Char, copySize);
    return event;
}

void FSTUFF_EventReceived(FSTUFF_Simulation *sim, FSTUFF_Event *event)
{
    switch (event->type) {
        case FSTUFF_EventKeyDown: {
            switch (std::toupper(event->data.key.utf8[0])) {
                case 'R': {
                    FSTUFF_ShutdownWorld(sim);
                    FSTUFF_InitWorld(sim);
                    event->handled = true;
                } break;
            }
        } break;
    }
}

void FSTUFF_CopyMatrix(matrix_float4x4 & dest, const gbMat4 & src)
{
    memcpy((void *)&(dest), (const void *)&(src.e), sizeof(float) * 16);
}

void FSTUFF_CopyMatrix(gbMat4 & dest, const matrix_float4x4 & src)
{
    memcpy((void *)&(dest.e), (const void *)&(src), sizeof(float) * 16);
}


void FSTUFF_Update(FSTUFF_Simulation * sim, FSTUFF_GPUData * gpuData)
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
    
    // Compute delta-time
    const double deltaTimeS = nowS - sim->lastUpdateUTCTimeS;
    sim->elapsedTimeS += deltaTimeS;
    
    // Add marbles, as warranted
    if (sim->marblesCount < sim->marblesMax) {
        if (sim->addMarblesInS > 0) {
            sim->addMarblesInS -= deltaTimeS;
            if (sim->addMarblesInS <= 0) {
                FSTUFF_AddMarble(sim);
                sim->addMarblesInS = FSTUFF_RandRangeF(sim->rng, sim->addMarblesInS_Range[0], sim->addMarblesInS_Range[1]);
            }
        }
    }
    
    // Update physics
    const cpFloat kSubstepTimeS = kStepTimeS / ((cpFloat)kNumSubSteps);
    while ((sim->lastUpdateUTCTimeS + kStepTimeS) <= nowS) {
        for (size_t i = 0; i < kNumSubSteps; ++i) {
            sim->lastUpdateUTCTimeS += kSubstepTimeS;
            cpSpaceStep(SPACE, kSubstepTimeS);
        }
    }
    
    // Reset world, if warranted
    if (sim->marblesCount >= sim->marblesMax) {
        if (sim->resetInS_default > 0) {
            if (sim->resetInS <= 0) {
                sim->resetInS = sim->resetInS_default;
            } else {
                sim->resetInS -= deltaTimeS;
            }
        }
        if (sim->resetInS <= 0) {
            sim->marblesCount = 0;
            FSTUFF_ShutdownWorld(sim);
            FSTUFF_InitWorld(sim);
        }
    }

    // Copy simulation/game data to GPU-accessible buffers
    //gpuData->globals.projection_matrix = sim->projectionMatrix;

//    gbMat4 m;
//    memcpy((void *)&(m.e), (void *)&(sim->projectionMatrix), sizeof(float) * 16);
//    memcpy((void *)&(gpuData->globals.projection_matrix), (void *)&(m.e), sizeof(float) * 16);


    FSTUFF_CopyMatrix(gpuData->globals.projection_matrix, sim->projectionMatrix);

    //memcpy((void *)&(gpuData->globals.projection_matrix), (void *)&(sim->projectionMatrix.e), sizeof(float) * 16);

    for (size_t i = 0; i < sim->world.numCircles; ++i) {
        cpFloat shapeRadius = cpCircleShapeGetRadius((cpShape*)CIRCLE(i));
        cpBody * body = cpShapeGetBody((cpShape*)CIRCLE(i));
        cpVect bodyCenter = cpBodyGetPosition(body);
        cpFloat bodyAngle = cpBodyGetAngle(body);
        
        gbMat4 dest, tmp;
        gb_mat4_identity(&dest);
        gb_mat4_translate(&tmp, {(float)bodyCenter.x, (float)bodyCenter.y, 0.});
        dest *= tmp;
        gb_mat4_rotate(&tmp, {0., 0., 1.}, bodyAngle);
        dest *= tmp;
        gb_mat4_scale(&tmp, {(float)shapeRadius, (float)shapeRadius, 1});
        dest *= tmp;

        FSTUFF_CopyMatrix(gpuData->circles[i].model_matrix, dest);
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

        gbMat4 dest, tmp;
        gb_mat4_identity(&dest);
        gb_mat4_translate(&tmp, {(float)bodyCenter.x, (float)bodyCenter.y, 0.});
        dest *= tmp;
        gb_mat4_rotate(&tmp, {0., 0., 1.}, bodyAngle);
        dest *= tmp;
        gb_mat4_translate(&tmp, {(float)center.x, (float)center.y, 0.});
        dest *= tmp;
        gb_mat4_rotate(&tmp, {0., 0., 1.}, cpvtoangle(b-a));
        dest *= tmp;
        gb_mat4_scale(&tmp, {(float)cpvlength(b-a), (float)(radius*2.), 1.});
        dest *= tmp;
        FSTUFF_CopyMatrix(gpuData->boxes[i].model_matrix, dest);

        gpuData->boxes[i].color = sim->world.boxColors[i];
    }
    
/*
	self.unlit_peg_fill_alpha_min = 0.25
	self.unlit_peg_fill_alpha_max = 0.45

 pb.fill_alpha = rand_in_range(self.unlit_peg_fill_alpha_min, self.unlit_peg_fill_alpha_max)
*/
}

void FSTUFF_Render(FSTUFF_Simulation * sim, FSTUFF_Renderer * renderer)
{
    FSTUFF_RenderShapes(renderer, &sim->circleFilled, 0,                  sim->world.numCircles,                      0.35f);
    FSTUFF_RenderShapes(renderer, &sim->circleDots,   sim->world.numPegs, sim->world.numCircles - sim->world.numPegs, 1.0f);
    FSTUFF_RenderShapes(renderer, &sim->circleEdged,  0,                  sim->world.numCircles,                      1.0f);
    FSTUFF_RenderShapes(renderer, &sim->boxFilled,    0,                  sim->world.numBoxes,                        0.35f);
    FSTUFF_RenderShapes(renderer, &sim->boxEdged,     0,                  sim->world.numBoxes,                        1.0f);
}

void FSTUFF_ViewChanged(FSTUFF_Simulation * sim, float widthMM, float heightMM)
{
    sim->viewSizeMM = {widthMM, heightMM};

    gbMat4 translation;
    gb_mat4_translate(&translation, {-1, -1, 0});
    
    gbMat4 scaling;
    gb_mat4_scale(&scaling, {2.0f / widthMM, 2.0f / heightMM, 1});
    
    sim->projectionMatrix = translation * scaling;
}

void FSTUFF_ShutdownWorld(FSTUFF_Simulation * sim)
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
    //cpSpaceDestroy(sim->world.physicsSpace);
    cpSpaceFree(sim->world.physicsSpace);
}

static void FSTUFF_ShutdownGPU(FSTUFF_Simulation * sim)
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

void FSTUFF_Shutdown(FSTUFF_Simulation * sim)
{
    FSTUFF_ShutdownWorld(sim);
    FSTUFF_ShutdownGPU(sim);
    sim->~FSTUFF_Simulation();
    ::operator delete(sim);
}

void FSTUFF_AddMarble(FSTUFF_Simulation * sim)
{
    cpBody * body = cpBodyInit(BODY_ALLOC(), 0, 0);
    cpSpaceAddBody(SPACE, body);
    const cpFloat marbleRadius = FSTUFF_RandRangeF(sim->rng, sim->marbleRadius_Range[0], sim->marbleRadius_Range[1]);
    cpBodySetPosition(body, cpv(FSTUFF_RandRangeF(sim->rng, marbleRadius, sim->viewSizeMM.x - marbleRadius), sim->viewSizeMM.y * 1.1));
    cpShape * shape = (cpShape*)cpCircleShapeInit(CIRCLE_ALLOC(), body, marbleRadius, cpvzero);
    cpSpaceAddShape(sim->world.physicsSpace, shape);
    cpShapeSetDensity(shape, 10);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    sim->world.circleColors[CIRCLE_IDX(shape)] = FSTUFF_Color(FSTUFF_Colors::White);
    sim->marblesCount += 1;
}

