//
//  FSTUFF.cpp
//  FallingStuff
//
//  Created by David Ludwig on 5/30/16.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

//#import "FSTUFF_AppleMetalStructs.h"

#include "FSTUFF.h"

#include <random>
#include <cstdarg>
#include <ctime>
#include <chrono>
#include <cctype>
#include <sstream>

// part of the utf8cpp library (at https://github.com/nemtrif/utfcpp)
#if (__clang__ || __GNUC__) && !__cpp_exceptions       // Some builds, such as for web/Emscripten, use -fno-exceptions
    #include "utf8/unchecked.h"
    namespace utf8_ = utf8::unchecked;
#else
    #include "utf8.h"
    namespace utf8_ = utf8;
#endif

#define GB_MATH_IMPLEMENTATION
#include "gb_math.h"

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

// #define FSTUFF_USE_DEBUG_PEGS 1



#pragma mark - Random Number Generation

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


#pragma mark - Misc

void FSTUFF_OpenWebPage(const char * url) {
#if __EMSCRIPTEN__
    char buf[2048];
    snprintf(buf, sizeof(buf), "window.open('%s','_self')", url);
    emscripten_run_script(buf);
#endif
}


#pragma mark - Rendering

FSTUFF_Renderer::~FSTUFF_Renderer()
{
}

constexpr gbVec4 FSTUFF_Color(uint32_t rgb, uint8_t a)
{
    return {
        ((((uint32_t)rgb) >> 16) & 0xFF) / 255.0f,
        ((((uint32_t)rgb) >> 8) & 0xFF) / 255.0f,
        (rgb & 0xFF) / 255.0f,
        (a) / 255.0f
    };
}

constexpr gbVec4 FSTUFF_Color(uint32_t rgb)
{
    return FSTUFF_Color(rgb, 0xff);
}


#pragma mark - Shapes, Circle

static const unsigned kNumCircleParts = 64; //32;

#define RAD_IDX(I) (((float)I) * kRadianStep)
#define COS_IDX(I) ((float)cos(RAD_IDX(I)))
#define SIN_IDX(I) ((float)sin(RAD_IDX(I)))

void FSTUFF_MakeCircleFilledTriangles(gbVec4 * vertices,
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

    const gbVec4 * verticesOrig = vertices;
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

void FSTUFF_MakeCircleTriangleStrip(gbVec4 * vertices, int maxVertices, int * numVertices, int numPartsToGenerate,
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

void FSTUFF_MakeCircleLineStrip(gbVec4 * vertices, int maxVertices, int * numVertices, int numPartsToGenerate,
                                float radius)
{
    // TODO: check the size of the vertex buffer!
    const float kRadianStep = ((((float)M_PI) * 2.0f) / (float)numPartsToGenerate);
    *numVertices = (numPartsToGenerate + 1);
    for (unsigned i = 0; i <= numPartsToGenerate; ++i) {
        vertices[i] = {COS_IDX(i)*radius, SIN_IDX(i)*radius, 0, 1};
    }
}

void FSTUFF_ShapeInit(FSTUFF_Shape * shape, FSTUFF_Renderer * renderer)
{
    // Generate vertices in CPU-accessible memory
//    vector_float4 vertices[2048];
    gbVec4 vertices[2048];
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

    //
    // Segments
    //
    else if (shape->type == FSTUFF_ShapeSegment) {
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


    else if (shape->type == FSTUFF_ShapeDebug) {
        didSet = true;

//        shape->primitiveType = FSTUFF_PrimitiveTriangles;
//        shape->numVertices = 3;
//        vertices[0] = {-0.5f, -0.5f, 0.f, 1.f};
//        vertices[1] = { 0.5f, -0.5f, 0.f, 1.f};
//        vertices[2] = { 0.5f,  0.5f, 0.f, 1.f};

        shape->primitiveType = FSTUFF_PrimitiveTriangleFan;
        shape->numVertices = 4;
        vertices[0] = {-.5f, -.5f, 0, 1};
        vertices[1] = {-.5f,  .5f, 0, 1};
        vertices[2] = { .5f, -.5f, 0, 1};
        vertices[3] = { .5f,  .5f, 0, 1};
    }

    if (didSet) {
        shape->gpuVertexBuffer = renderer->NewVertexBuffer(vertices,
                                                           shape->numVertices * sizeof(gbVec4));
    } else {
        shape->gpuVertexBuffer = NULL;
    }
}


#pragma mark - Simulation

static const cpFloat kPhysicsStepTimeS = 1./600.;
static const cpFloat kMaxDeltaTimeS = 1.0;


#define SPACE           (this->physicsSpace)

void FSTUFF_Simulation::InitGPUShapes()
{
    //
    // GPU init
    //
    this->circleFilled.debugName = "FSTUFF_CircleFilled";
    this->circleFilled.type = FSTUFF_ShapeCircle;
    this->circleFilled.appearance = FSTUFF_ShapeAppearanceFilled;
    this->circleFilled.circle.numParts = kNumCircleParts;
    FSTUFF_ShapeInit(&(this->circleFilled), this->renderer);

    this->circleDots.debugName = "FSTUFF_CircleDots";
    this->circleDots.type = FSTUFF_ShapeCircle;
    this->circleDots.appearance = FSTUFF_ShapeAppearanceFilled;
    this->circleDots.circle.numParts = kNumCircleParts;
    this->circleDots.primitiveType = FSTUFF_PrimitiveTriangles;
    {
        gbVec4 vertices[2048];
        const int numCirclePartsForDot = 6;
        const float dotRadius = 0.08f;  // size of dot: 0 to 1; 0 is no-size, 1 is as big as containing-circle
        const float dotDistance = 0.7f; // from 0 to 1
        int tmpVertexCount;
        this->circleDots.numVertices = 0;
        for (int i = 0, n = 6; i < n; ++i) {
            const float rad = float(i) * ((M_PI * 2.f) / float(n));
            FSTUFF_MakeCircleFilledTriangles(
                &vertices[this->circleDots.numVertices],
                0,
                &tmpVertexCount,
                numCirclePartsForDot,
                dotRadius,
                cosf(rad) * dotDistance,
                sinf(rad) * dotDistance
            );
            this->circleDots.numVertices += tmpVertexCount;
        }
        this->circleDots.gpuVertexBuffer = this->renderer->NewVertexBuffer(vertices, (this->circleDots.numVertices * sizeof(gbVec4)));
    }

    this->circleEdged.debugName = "FSTUFF_CircleEdged";
    this->circleEdged.type = FSTUFF_ShapeCircle;
    this->circleEdged.appearance = FSTUFF_ShapeAppearanceEdged;
    this->circleEdged.circle.numParts = kNumCircleParts;
    FSTUFF_ShapeInit(&(this->circleEdged), this->renderer);

    this->boxFilled.debugName = "FSTUFF_BoxEdged";
    this->boxFilled.type = FSTUFF_ShapeBox;
    this->boxFilled.appearance = FSTUFF_ShapeAppearanceFilled;
    FSTUFF_ShapeInit(&(this->boxFilled), this->renderer);
    
    this->boxEdged.debugName = "FSTUFF_BoxEdged";
    this->boxEdged.type = FSTUFF_ShapeBox;
    this->boxEdged.appearance = FSTUFF_ShapeAppearanceEdged;
    FSTUFF_ShapeInit(&(this->boxEdged), this->renderer);

    this->segmentFilled.debugName = "FSTUFF_SegmentEdged";
    this->segmentFilled.type = FSTUFF_ShapeSegment;
    this->segmentFilled.appearance = FSTUFF_ShapeAppearanceFilled;
    FSTUFF_ShapeInit(&(this->segmentFilled), this->renderer);

    this->segmentEdged.debugName = "FSTUFF_SegmentEdged";
    this->segmentEdged.type = FSTUFF_ShapeSegment;
    this->segmentEdged.appearance = FSTUFF_ShapeAppearanceEdged;
    FSTUFF_ShapeInit(&(this->segmentEdged), this->renderer);

    this->debugShape.debugName = "FSTUFF_DebugShape";
    this->debugShape.type = FSTUFF_ShapeDebug;
    this->debugShape.appearance = FSTUFF_ShapeAppearanceFilled;
    FSTUFF_ShapeInit(&(this->debugShape), this->renderer);
}

void FSTUFF_Simulation::InitWorld()
{
    //
    // Physics-world init
    //
    memset(&circles, 0, sizeof(circles));
    memset(&circleColors, 0, sizeof(circleColors));
    memset(&boxes, 0, sizeof(boxes));
    memset(&boxColors, 0, sizeof(boxColors));
    memset(&segments, 0, sizeof(segments));
    memset(&segmentColors, 0, sizeof(segmentColors));
    memset(&bodies, 0, sizeof(bodies));
    this->physicsSpace = cpSpaceNew();
    
    cpSpaceSetIterations(this->physicsSpace, 2);
    cpSpaceSetGravity(this->physicsSpace, this->game.gravity);
    // TODO: try resizing cpSpace hashes
    //cpSpaceUseSpatialHash(this->world.physicsSpace, 2, 10);

    cpBody * body;
    cpShape * shape;
    
    
    //
    // Walls
    //

    body = cpBodyInit(NewBody(), 0, 0);
    cpBodySetType(body, CP_BODY_TYPE_STATIC);
    cpSpaceAddBody(SPACE, body);
    cpBodySetPosition(body, cpv(0, 0));
    static const cpFloat wallThickness = 5.0;
    static const cpFloat wallLeft   = -wallThickness / 2.;
    static const cpFloat wallRight  = this->GetWorldWidth() + (wallThickness / 2.);
    static const cpFloat wallBottom = -wallThickness / 2.;
    static const cpFloat wallTop    = this->GetWorldHeight() * 2.;   // use a high ceiling, to make sure off-screen falling things don't go over walls
    
#if ! FSTUFF_USE_DEBUG_PEGS
    // Bottom
    shape = (cpShape*)cpSegmentShapeInit(NewSegment(), body, cpv(wallLeft,wallBottom), cpv(wallRight,wallBottom), wallThickness/2.);
    cpSpaceAddShape(SPACE, shape);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    this->segmentColors[IndexOfSegment(shape)] = FSTUFF_Color(0x000000, 0x00);
    // Left
    shape = (cpShape*)cpSegmentShapeInit(NewSegment(), body, cpv(wallLeft,wallBottom), cpv(wallLeft,wallTop), wallThickness/2.);
    cpSpaceAddShape(SPACE, shape);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    this->segmentColors[IndexOfSegment(shape)] = FSTUFF_Color(0x000000, 0x00);
    // Right
    shape = (cpShape*)cpSegmentShapeInit(NewSegment(), body, cpv(wallRight,wallBottom), cpv(wallRight,wallTop), wallThickness/2.);
    cpSpaceAddShape(SPACE, shape);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    this->segmentColors[IndexOfSegment(shape)] = FSTUFF_Color(0x000000, 0x00);
#endif


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
#if FSTUFF_USE_DEBUG_PEGS
//    const int numPegs = 1;
    const int numPegs = 2;
#else
    const int numPegs = round((this->GetWorldWidth() * this->GetWorldHeight()) * 0.0005);
#endif
    const cpFloat kPegScaleCircle = 2.5;
    const cpFloat kPegScaleBox = 4.;
    cpFloat cx, cy, radius, w, h, angleRad;
    int pegColorIndex;
    for (int i = 0; i < numPegs; ++i) {
#if FSTUFF_USE_DEBUG_PEGS
        // const int which_peg_type = 0;
        const int which_peg_type = 1;
#else
        const int which_peg_type = rand() % 2;
#endif
        switch (which_peg_type) {
            case 0:
            {
#if FSTUFF_USE_DEBUG_PEGS
                cx = (this->GetWorldWidth() / 4.) + (i * (this->GetWorldWidth() / 2));
                cy = this->GetWorldHeight() / 2.;
                radius = (this->GetWorldWidth() / 4.) + 10.f;
                pegColorIndex = 0;
#else
                cx = FSTUFF_RandRangeF(this->game.rng, 0., this->GetWorldWidth());
                cy = FSTUFF_RandRangeF(this->game.rng, 0., this->GetWorldHeight());
                radius = kPegScaleCircle * FSTUFF_RandRangeF(this->game.rng, 6., 10.);
                pegColorIndex = FSTUFF_RandRangeI(this->game.rng, 0, FSTUFF_countof(pegColors)-1);
#endif

                body = cpBodyInit(NewBody(), 0, 0);
                cpBodySetType(body, CP_BODY_TYPE_STATIC);
                cpSpaceAddBody(SPACE, body);
                cpBodySetPosition(body, cpv(cx, cy));
                shape = (cpShape*)cpCircleShapeInit(NewCircle(), body, radius, cpvzero);
                ++this->game.numPegs;
                cpSpaceAddShape(SPACE, shape);
                cpShapeSetElasticity(shape, 0.8);
                cpShapeSetFriction(shape, 1);
#if FSTUFF_USE_DEBUG_PEGS
                switch (i % 2) {
                    case 0:
                        this->circleColors[IndexOfCircle(shape)] = FSTUFF_Color(Blue);
                        break;
                    case 1:
                        this->circleColors[IndexOfCircle(shape)] = FSTUFF_Color(Green);
                        break;
                }
#else
                this->circleColors[IndexOfCircle(shape)] = FSTUFF_Color(pegColors[pegColorIndex]);
#endif
            } break;
            
            case 1:
            {
#if FSTUFF_USE_DEBUG_PEGS
                cx = this->GetWorldWidth() / 2.;
                cy = this->GetWorldHeight() / 2.;
                w = 20;
                h = 50;
                angleRad = M_PI / 2.;
                pegColorIndex = 0;
#else
                cx = FSTUFF_RandRangeF(this->game.rng, 0., this->GetWorldWidth());
                cy = FSTUFF_RandRangeF(this->game.rng, 0., this->GetWorldHeight());
                w = kPegScaleBox * FSTUFF_RandRangeF(this->game.rng, 6., 14.);
                h = kPegScaleBox * FSTUFF_RandRangeF(this->game.rng, 1., 2.);
                angleRad = FSTUFF_RandRangeF(this->game.rng, 0., M_PI);
                pegColorIndex = FSTUFF_RandRangeI(this->game.rng, 0, FSTUFF_countof(pegColors)-1);
#endif

                body = cpBodyInit(NewBody(), 0, 0);
                cpBodySetType(body, CP_BODY_TYPE_STATIC);
                cpSpaceAddBody(SPACE, body);
                cpBodySetPosition(body, cpv(cx, cy));
                cpBodySetAngle(body, angleRad);
                shape = (cpShape*)cpBoxShapeInit(NewBox(), body, w, h, 0.);
                cpSpaceAddShape(SPACE, shape);
                cpShapeSetElasticity(shape, 0.8);
                this->boxColors[IndexOfBox(shape)] = FSTUFF_Color(pegColors[pegColorIndex]);
            } break;
        }
    }
    
//    for (int i = 0; i < 1500; i++) {
//        this->AddMarble();
//    }
}

void FSTUFF_Simulation::AddMarble()
{
    FSTUFF_Simulation * sim = this;
    cpBody * body = cpBodyInit(NewBody(), 0, 0);
    cpSpaceAddBody(SPACE, body);
    const cpFloat marbleRadius = FSTUFF_RandRangeF(sim->game.rng, sim->game.marbleRadius_Range[0], sim->game.marbleRadius_Range[1]);
    cpBodySetPosition(body, cpv(FSTUFF_RandRangeF(sim->game.rng, marbleRadius, sim->GetWorldWidth() - marbleRadius), sim->GetWorldHeight() * 1.1));
    cpShape * shape = (cpShape*)cpCircleShapeInit(NewCircle(), body, marbleRadius, cpvzero);
    cpSpaceAddShape(sim->physicsSpace, shape);
    cpShapeSetDensity(shape, 10);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    sim->circleColors[IndexOfCircle(shape)] = FSTUFF_Color(FSTUFF_Colors::White);
    sim->game.marblesCount += 1;
}


bool FSTUFF_Simulation::DidInit() const
{
    return (this->state != FSTUFF_DEAD);
}

FSTUFF_Simulation::FSTUFF_Simulation() {
    FSTUFF_Log("%s, this:%p\n", __FUNCTION__, this);
}

FSTUFF_Simulation::~FSTUFF_Simulation() {
    FSTUFF_Log("%s, this:%p, this->state:%d\n", __FUNCTION__, this, this->state);
}


void FSTUFF_DefaultFatalErrorHandler(const char * formattedMessage, void *) {
    fprintf(stderr, "%s\n", formattedMessage);
    std::abort();
}

void FSTUFF_FatalError_Inner(FSTUFF_CodeLocation codeLocation, const char * fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    char intermediateFormat[2048];
    snprintf(
        intermediateFormat,
        FSTUFF_countof(intermediateFormat),
        "\nFATAL ERROR:\n   Function: %s\n       Line: %d\n       File: %s:%d\n    Message: \%s\n",
        (codeLocation.functionName ? codeLocation.functionName : ""),
        codeLocation.line,
        (codeLocation.fileName ? codeLocation.fileName : ""),
        codeLocation.line,
        (fmt ? fmt : ""));
    char finalMessage[2048];
    vsnprintf(finalMessage, FSTUFF_countof(finalMessage), intermediateFormat, arg);
    FSTUFF_DefaultFatalErrorHandler(finalMessage, nullptr);
    va_end(arg);
}

void FSTUFF_Simulation::Init() //, void * gpuDevice, void * nativeView)
{
    FSTUFF_Log("%s, this:%p, state:%d, renderer:%p\n",
        __FUNCTION__, this, this->state, this->renderer);

    // FSTUFF_FatalError("uh oh: %d", 123);
    
    if ( ! this->renderer) {
        FSTUFF_FatalError("FSTUFF_Simulation's 'renderer' field must be set, before calling its Init() method!");
    }

    if ( ! this->imGuiContext) {
        this->imGuiContext = ImGui::CreateContext();
        ImGui::SetCurrentContext(this->imGuiContext);
        ImGuiIO & io = ImGui::GetIO();

        // Prevent ImGui::NewFrame from performing file I/O.  This
        // can help lead to a smaller binary when compiling with
        // Emscripten, when used in conjunction with the build
        // setting, '-s FILESYSTEM=0'.
        io.IniFilename = nullptr;
    }
    
    // Don't re-initialize simulations that are already alive
    if (this->state != FSTUFF_DEAD) {
        return;
    }
    
    srand(time(nullptr));

    // Reset relevant variables (in 'this->game')
    game = FSTUFF_Simulation::Resettable();

    // Mark simulation as alive
    this->state = FSTUFF_ALIVE;

    // Initialize 'this'
    this->keysPressed.reset();  // Mark all keys as being down/un-pressed
    FSTUFF_Assert(this->viewSize.widthPixels > 0);
    FSTUFF_Assert(this->viewSize.heightPixels > 0);
    this->InitGPUShapes();
    this->InitWorld();
}

void FSTUFF_Simulation::ResetWorld()
{
    this->ShutdownWorld();
    this->game = FSTUFF_Simulation::Resettable();
    this->InitWorld();
}

void FSTUFF_Simulation::Update()
{
    // Initialize the simulation, if need be.
    if (this->state == FSTUFF_DEAD) {
//        this->renderer->ViewChanged();
        this->Init();
    }

    // Compute current time
    //
    // nowS == current time, in seconds since UNIX epoch
    const cpFloat nowS =
        static_cast<cpFloat>(std::chrono::high_resolution_clock::now().time_since_epoch().count()) *
        ((cpFloat) std::chrono::high_resolution_clock::period::num / (cpFloat) std::chrono::high_resolution_clock::period::den);

    // Initialize simulation time vars, on first tick
    if (this->game.lastUpdateUTCTimeS == 0.) {
        this->game.lastUpdateUTCTimeS = nowS;
    }

    // Compute delta-time, adjusting it down to kMaxDeltaTimeS as necessary.
    // Adjustments down to kMaxDeltaTimeS are done as a sort of fix whereby
    // large delta-time values, such as those generated when hiding and resuming
    // an app (or web-page!).
    double deltaTimeS = nowS - this->game.lastUpdateUTCTimeS;
    if (deltaTimeS > kMaxDeltaTimeS) {
        // const auto oldUpdateTimeS = this->game.lastUpdateUTCTimeS;
        // const auto oldDtS = deltaTimeS;
        this->game.lastUpdateUTCTimeS = nowS - kMaxDeltaTimeS;
        deltaTimeS = kMaxDeltaTimeS;
        // FSTUFF_Log("... adjusted last-update time (seconds) down to %f (from %f; now=%f)\n",
        //     this->game.lastUpdateUTCTimeS, oldUpdateTimeS, nowS);
        // FSTUFF_Log("... adjusted dt (seconds) down to %f (from %f)\n", deltaTimeS, oldDtS);
    }
    this->game.elapsedTimeS += deltaTimeS;

    // Rendering-initialization.  This is done *BEFORE* ImGui calls start,
    // which may involve texture-creation.  (Is this necessary?)
    this->renderer->BeginFrame();

    // Update ImGui's low-level state
    ImGuiIO & io = ImGui::GetIO();
    if ( ! io.Fonts->TexID) {
        unsigned char *pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height); // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
        io.Fonts->TexID = this->renderer->NewTexture(pixels, width, height);
    }
    io.DeltaTime = deltaTimeS;
    io.DisplaySize.x = this->viewSize.widthOS;
    io.DisplaySize.y = this->viewSize.heightOS;
    io.DisplayFramebufferScale.x = (float)this->viewSize.widthPixels / (float)this->viewSize.widthOS;
    io.DisplayFramebufferScale.y = (float)this->viewSize.heightPixels / (float)this->viewSize.heightOS;
    const FSTUFF_CursorInfo cursorPos = this->renderer->GetCursorInfo();
    io.MousePos.x = cursorPos.xOS;
    io.MousePos.y = cursorPos.yOS;
    io.MouseDown[0] = cursorPos.pressed;
    ImGui::NewFrame();
    ImGui::StyleColorsDark(&ImGui::GetStyle());

#if FSTUFF_ENABLE_IMGUI_DEMO
    if (this->showGUIDemo) {
        ImGui::ShowDemoWindow();
    }
#endif

    // Add marbles, as warranted
    if (this->game.marblesCount < this->marblesMax) {
        if (this->addNumMarblesPerSecond > 0) {
            this->game.addMarblesInS -= deltaTimeS;
            if (this->game.addMarblesInS <= 0) {
                this->AddMarble();
                this->game.addMarblesInS = 1.f / this->addNumMarblesPerSecond;
            }
        }
    }

    // Update physics
    while ((this->game.lastUpdateUTCTimeS + kPhysicsStepTimeS) <= nowS) {
        cpSpaceStep(SPACE, kPhysicsStepTimeS);
        this->game.lastUpdateUTCTimeS += kPhysicsStepTimeS;
    }

    // Reset world, if warranted
    if (this->game.marblesCount >= this->marblesMax) {
        if (this->game.resetInS_default > 0) {
            if (this->game.resetInS <= 0) {
                this->game.resetInS = this->game.resetInS_default;
            } else {
                this->game.resetInS -= deltaTimeS;
            }
        }
        if (this->game.resetInS <= 0) {
            this->ResetWorld();
        }
    }
    
    // Process GUI
    if (this->showSettings) {
//        ImGui::SetNextWindowSize(ImVec2(450, 200));
//        ImGui::Begin("Settings", NULL, ImVec2(500, 200));
        bool * closeBoxState = NULL;
        if (this->configurationMode) {
            closeBoxState = NULL;   // Don't show the close box, in Screen Saver configuration mode.
        } else {
            closeBoxState = &this->showSettings;
        }
        ImGui::Begin("Settings", closeBoxState, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::SliderInt("Marbles, Max", &this->marblesMax, 0, 1000);
        if (ImGui::SliderFloat("Spawn Rate (marbles/second)", &this->addNumMarblesPerSecond, 0, 10, "%.3f", 3.0f)) {
            this->game.addMarblesInS = 1.f / this->addNumMarblesPerSecond;
        }
        ImGui::InvisibleButton("padding1", ImVec2(8, 8));
        ImGui::Separator();
        ImGui::InvisibleButton("padding2", ImVec2(8, 8));
        if (ImGui::Button("Restart Simulation", ImVec2(400, 32))) {
            this->ResetWorld();
        }
        if (this->configurationMode) {
            ImGui::InvisibleButton("padding1", ImVec2(8, 8));
            ImGui::Separator();
            ImGui::InvisibleButton("padding2", ImVec2(8, 8));
            if (ImGui::Button("OK", ImVec2(400, 32))) {
//                FSTUFF_Log("%s, doEndConfiguration set to true for sim:%p\n", __PRETTY_FUNCTION__, this);
                this->doEndConfiguration = true;
            }
        }
        ImGui::InvisibleButton("padding2", ImVec2(8, 8));
        ImGui::Separator();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("written by David Lee Ludwig");
        ImGui::SameLine();
        if (ImGui::Button("http://dll.software")) {
            FSTUFF_OpenWebPage("http://dll.software");
        }
        ImGui::End();
    }

    // Copy simulation/game data to GPU-accessible buffers
    this->renderer->SetProjectionMatrix(this->projectionMatrix);
    for (size_t i = 0; i < this->game.numCircles; ++i) {
        cpFloat shapeRadius = cpCircleShapeGetRadius((cpShape*)GetCircle(i));
        cpBody * body = cpShapeGetBody((cpShape*)GetCircle(i));
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

        this->renderer->SetShapeProperties(FSTUFF_ShapeCircle, i, dest, this->circleColors[i]);
    }
    for (size_t i = 0; i < this->game.numBoxes; ++i) {
        FSTUFF_Assert(cpPolyShapeGetCount((cpShape*)GetBox(i)) == 4);
        const cpVect bottomRight = cpPolyShapeGetVert((cpShape*)GetBox(i), 0);
        const cpVect topRight    = cpPolyShapeGetVert((cpShape*)GetBox(i), 1);
        const cpVect topLeft     = cpPolyShapeGetVert((cpShape*)GetBox(i), 2);
        const auto w = topRight.x - topLeft.x;
        FSTUFF_Assert(w >= 0);
        const auto h = topRight.y - bottomRight.y;
        FSTUFF_Assert(h >= 0);
        const cpBody * body      = cpShapeGetBody((cpShape*)GetBox(i));
        const cpVect bodyCenter  = cpBodyGetPosition(body);
        const cpFloat bodyAngle  = cpBodyGetAngle(body);

        gbMat4 dest, tmp;
        gb_mat4_identity(&dest);
        gb_mat4_translate(&tmp, {(float)bodyCenter.x, (float)bodyCenter.y, 0.});
        dest *= tmp;
        gb_mat4_rotate(&tmp, {0., 0., 1.}, bodyAngle);
        dest *= tmp;
        gb_mat4_scale(&tmp, {(float)w, (float)h, 1.});
        dest *= tmp;

        this->renderer->SetShapeProperties(FSTUFF_ShapeBox, i, dest, this->boxColors[i]);
    }
    for (size_t i = 0; i < this->game.numSegments; ++i) {
        cpVect a = cpSegmentShapeGetA((cpShape*)GetSegment(i));
        cpVect b = cpSegmentShapeGetB((cpShape*)GetSegment(i));
        cpVect center = cpvlerp(a, b, 0.5);
        cpFloat radius = cpSegmentShapeGetRadius((cpShape*)GetSegment(i));
        cpBody * body = cpShapeGetBody((cpShape*)GetSegment(i));
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
        
        this->renderer->SetShapeProperties(FSTUFF_ShapeSegment, i, dest, this->segmentColors[i]);
    }


#if FSTUFF_USE_DEBUG_PEGS
    {
        gbMat4 dest, tmp;
        gb_mat4_identity(&dest);
        gb_mat4_translate(&tmp, {20., 50., 0.});
        dest *= tmp;
        gb_mat4_rotate(&tmp, {0., 0., 1.}, 0.);
        dest *= tmp;
        gb_mat4_scale(&tmp, {20., 20., 1.});
        dest *= tmp;

        this->renderer->SetShapeProperties(FSTUFF_ShapeDebug, 0, dest, FSTUFF_Color(0x00ff00));
    }
#endif
    
/*
	self.unlit_peg_fill_alpha_min = 0.25
	self.unlit_peg_fill_alpha_max = 0.45

 pb.fill_alpha = rand_in_range(self.unlit_peg_fill_alpha_min, self.unlit_peg_fill_alpha_max)
*/
}

void FSTUFF_Simulation::Render()
{
    renderer->RenderShapes(&circleFilled, 0,            game.numCircles,                0.35f);
    renderer->RenderShapes(&circleDots,   game.numPegs, game.numCircles - game.numPegs, 1.0f);
    renderer->RenderShapes(&circleEdged,  0,            game.numCircles,                1.0f);
    renderer->RenderShapes(&boxFilled,    0,            game.numBoxes,                  0.35f);
    renderer->RenderShapes(&boxEdged,     0,            game.numBoxes,                  1.0f);
    renderer->RenderShapes(&segmentFilled,0,            game.numSegments,               0.35f);
    renderer->RenderShapes(&segmentEdged, 0,            game.numSegments,               1.0f);

#if FSTUFF_USE_DEBUG_PEGS
    renderer->RenderShapes(&debugShape, 0, 1, 0.5678);
#endif

    ImGui::EndFrame();
    ImGui::Render();
}

void FSTUFF_Simulation::ViewChanged(const FSTUFF_ViewSize & viewSize)
{
    this->viewSize = viewSize;
    this->UpdateProjectionMatrix();
    FSTUFF_Assert(this->renderer);
    this->renderer->ViewChanged();
}

void FSTUFF_Simulation::SetGlobalScale(cpVect scale)
{
    this->globalScale = scale;
    this->UpdateProjectionMatrix();
}

void FSTUFF_Simulation::UpdateProjectionMatrix()
{
    FSTUFF_Log("%s: viewSize=%f,%f (mm); globalScale=%f,%f\n",
        __FUNCTION__,
        this->viewSize.widthMM, this->viewSize.heightMM,
        this->globalScale.x, this->globalScale.y
    );

    //
    // Start by setting projectionMatrix to an identity matrix,
    // then apply transformations as directly as possible to it.
    //
    gb_mat4_identity(&this->projectionMatrix);

    gbMat4 tmp;

    gb_mat4_translate(&tmp, {-1, -1, 0});
    this->projectionMatrix *= tmp;
    
    gb_mat4_scale(&tmp, {
        (float)(2.0f / this->viewSize.widthMM),
        (float)(2.0f / this->viewSize.heightMM),
        1
    });
    this->projectionMatrix *= tmp;

    gb_mat4_scale(&tmp, {
        (float)(this->globalScale.x),
        (float)(this->globalScale.y),
        1
    });
    this->projectionMatrix *= tmp;

    gb_mat4_translate(&tmp, this->game.viewTranslation);
    this->projectionMatrix *= tmp;
}

void FSTUFF_Simulation::UpdateCursorInfo(const FSTUFF_CursorInfo & newInfo)
{
    const FSTUFF_CursorInfo oldInfo = this->cursorInfo;
    const bool doSendEvents = (this->state != FSTUFF_DEAD);

    if (newInfo.pressed != oldInfo.pressed) {
        this->cursorInfo.pressed = newInfo.pressed;

        if (doSendEvents) {
            FSTUFF_Event event;
            memset(&event, 0, sizeof(event));
            event.type = FSTUFF_CursorButton;
            event.data.cursorButton.xOS = this->cursorInfo.xOS;
            event.data.cursorButton.yOS = this->cursorInfo.yOS;
            event.data.cursorButton.down = this->cursorInfo.pressed;
            this->EventReceived(&event);
        }
    }

    if (newInfo.xOS != oldInfo.xOS || newInfo.yOS != oldInfo.yOS) {
        this->cursorInfo.xOS = newInfo.xOS;
        this->cursorInfo.yOS = newInfo.yOS;

        if (doSendEvents) {
            FSTUFF_Event event;
            memset(&event, 0, sizeof(event));
            event.type = FSTUFF_CursorMotion;
            event.data.cursorMotion.xOS = this->cursorInfo.xOS;
            event.data.cursorMotion.yOS = this->cursorInfo.yOS;
            this->EventReceived(&event);
        }
    }
}

void FSTUFF_Simulation::ShutdownWorld()
{
    for (size_t i = 0; i < this->game.numCircles; ++i) {
        cpShapeDestroy((cpShape*)GetCircle(i));
    }
    for (size_t i = 0; i < this->game.numBoxes; ++i) {
        cpShapeDestroy((cpShape*)GetBox(i));
    }
    for (size_t i = 0; i < this->game.numSegments; ++i) {
        cpShapeDestroy((cpShape*)GetSegment(i));
    }
    for (size_t i = 0; i < this->game.numBodies; ++i) {
        cpBodyDestroy(GetBody(i));
    }
//    cpSpaceDestroy(this->world.physicsSpace);
    if (this->physicsSpace) {
        cpSpaceFree(this->physicsSpace);
        this->physicsSpace = nullptr;
    }
}

void FSTUFF_Simulation::ShutdownGPU()
{
    if (this->circleDots.gpuVertexBuffer) {
        this->renderer->DestroyVertexBuffer(this->circleDots.gpuVertexBuffer);
    }
    if (this->circleEdged.gpuVertexBuffer) {
        this->renderer->DestroyVertexBuffer(this->circleEdged.gpuVertexBuffer);
    }
    if (this->circleFilled.gpuVertexBuffer) {
        this->renderer->DestroyVertexBuffer(this->circleFilled.gpuVertexBuffer);
    }
    if (this->boxEdged.gpuVertexBuffer) {
        this->renderer->DestroyVertexBuffer(this->boxEdged.gpuVertexBuffer);
    }
    if (this->boxFilled.gpuVertexBuffer) {
        this->renderer->DestroyVertexBuffer(this->boxFilled.gpuVertexBuffer);
    }
    if (this->segmentEdged.gpuVertexBuffer) {
        this->renderer->DestroyVertexBuffer(this->segmentEdged.gpuVertexBuffer);
    }
    if (this->segmentFilled.gpuVertexBuffer) {
        this->renderer->DestroyVertexBuffer(this->segmentFilled.gpuVertexBuffer);
    }
}

//void FSTUFF_Shutdown(FSTUFF_Simulation * sim)
//{
//    sim->ShutdownWorld();
//    sim->ShutdownGPU();
//    sim->~FSTUFF_Simulation();
//    ::operator delete(sim);
//}


#pragma mark - Input Events

std::string FSTUFF_Event::KeyToUTF8() const
{
    std::string result;
    utf8_::utf32to8(&this->data.key.utf32, (&this->data.key.utf32) + 1, std::back_inserter(result));
    return result;
}

FSTUFF_Event FSTUFF_Event::NewKeyEvent(FSTUFF_EventType eventType, const char * utf8Char)
{
    std::vector<char32_t> temp;
    std::string_view src(utf8Char);
    utf8_::utf8to32(src.begin(), src.end(), std::back_inserter(temp));
    FSTUFF_Assert(temp.size() >= 1);
    return NewKeyEvent(eventType, temp[0]);
}

FSTUFF_Event FSTUFF_Event::NewKeyEvent(FSTUFF_EventType keyEventType, char32_t utf32Char)
{
    FSTUFF_Event event;
    memset(&event, 0, sizeof(event));
    event.type = keyEventType;
    event.data.key.utf32 = utf32Char;
    return event;
}


//FSTUFF_Event FSTUFF_Event::NewCursorMotionEvent(float xOS, float yOS)
//{
//    FSTUFF_Event event;
//    memset(&event, 0, sizeof(event));
//    event.type = FSTUFF_CursorMotion;
//    event.data.cursorMotion.xOS = xOS;
//    event.data.cursorMotion.yOS = yOS;
//    return event;
//}
//
//FSTUFF_Event FSTUFF_Event::NewCursorButtonEvent(float xOS, float yOS, bool down)
//{
//    FSTUFF_Event event;
//    memset(&event, 0, sizeof(event));
//    event.type = FSTUFF_CursorButton;
//    event.data.cursorButton.xOS = xOS;
//    event.data.cursorButton.yOS = yOS;
//    event.data.cursorButton.down = down;
//    return event;
//}
//
//FSTUFF_Event FSTUFF_Event::NewCursorContainedEvent(bool contained)
//{
//    FSTUFF_Event event;
//    memset(&event, 0, sizeof(event));
//    event.type = FSTUFF_CursorContained;
//    event.data.cursorContain.contained = contained;
//    return event;
//}

void FSTUFF_Simulation::EventReceived(FSTUFF_Event *event)
{
//    ImGuiIO& io = ImGui::GetIO();
//    if (action == GLFW_PRESS)
//        io.KeysDown[key] = true;
//    if (action == GLFW_RELEASE)
//        io.KeysDown[key] = false;
//
//    (void)mods; // Modifiers are not reliable across systems
//    io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
//    io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
//    io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
//    io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];

    ImGuiIO &guiIO = ImGui::GetIO();

    switch (event->type) {
        case FSTUFF_EventNone: {
        } break;

        case FSTUFF_EventKeyDown: {
            const std::string keyUTF8 = event->KeyToUTF8();
            if (keyUTF8[0] >= 0 && keyUTF8[0] <= 127) {
                const auto key = keyUTF8[0];
                this->keysPressed[key] = 1;
                guiIO.KeysDown[key] = 1;
            }
            bool unhandled = false;
            switch (std::toupper(event->KeyToUTF32())) {
                case U'D': {
                    this->showGUIDemo = !this->showGUIDemo;
                } break;
                case U'R': {
                    this->ResetWorld();
                } break;
                case U'S': {
                    if ( ! this->configurationMode) {
                        this->showSettings = !this->showSettings;
                    }
                } break;
                case U'←': {
                    this->game.viewTranslation.x -= 5.;
                    this->UpdateProjectionMatrix();
                } break;
                case U'→': {
                    this->game.viewTranslation.x += 5.;
                    this->UpdateProjectionMatrix();
                } break;
                case U'↓': {
                    this->game.viewTranslation.y -= 5.;
                    this->UpdateProjectionMatrix();
                } break;
                case U'↑': {
                    this->game.viewTranslation.y += 5.;
                    this->UpdateProjectionMatrix();
                } break;
                default: {
                    unhandled = true;
                } break;
            }
            if (!unhandled) {
                event->handled = true;
            }
        } break;

        case FSTUFF_EventKeyUp: {
            const std::string keyUTF8 = event->KeyToUTF8();
            if (keyUTF8[0] >= 0 && keyUTF8[0] <= 127) {
                const auto key = keyUTF8[0];
                this->keysPressed[key] = 0;
                guiIO.KeysDown[key] = 0;
            }
        } break;
        
        case FSTUFF_CursorButton: {
            // FSTUFF_Log("FSTUFF_CursorButton: os position = {%f,%f}, down?=%s\n", event->data.cursorButton.xOS, event->data.cursorButton.yOS, (event->data.cursorButton.down ? "YES" : "NO"));
        } break;

        case FSTUFF_CursorMotion: {
            // FSTUFF_Log("FSTUFF_CursorMotion: os position = {%f,%f}\n", event->data.cursorMotion.xOS, event->data.cursorMotion.yOS);
        } break;
        
        case FSTUFF_CursorContained: {
            FSTUFF_Log("FSTUFF_CursorContained: contained?=%s\n", (event->data.cursorContain.contained ? "YES" : "NO"));
        } break;
    }
}
