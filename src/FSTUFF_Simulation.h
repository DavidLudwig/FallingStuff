//
//  FSTUFF.h
//  FallingStuff
//
//  Created by David Ludwig on 5/30/16.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

// #ifndef FSTUFF_Simulation_hpp
// #define FSTUFF_Simulation_hpp

// #include "FSTUFF.h"

static const cpFloat kPhysicsStepTimeS = 1./600.;

template <typename FSTUFF_RendererType>
struct FSTUFF_Simulation {
    FSTUFF_SimulationState state = FSTUFF_DEAD;

    //
    // Geometry + GPU
    //
    FSTUFF_Shape circleFilled;
    FSTUFF_Shape circleDots;
    FSTUFF_Shape circleEdged;
    FSTUFF_Shape boxFilled;
    FSTUFF_Shape boxEdged;
    FSTUFF_Shape debugShape;
    gbMat4 projectionMatrix;
    //cpVect viewSizeMM = {0, 0};
    FSTUFF_ViewSize viewSize;
    FSTUFF_RendererType * renderer = NULL;
    
    struct Resettable {
        //
        // Random Number Generation
        //
        std::mt19937 rng;

        //
        // Timing
        //
        cpFloat lastUpdateUTCTimeS = 0.0;       // set on FSTUFF_Update; UTC time in seconds
        double elapsedTimeS = 0.0;              // elapsed time, in seconds; 0 == no time has passed

        //
        // Misc parameters
        //
        float addMarblesInS             = 0.0f;
        float addNumMarblesPerSecond    = 1.0f;
        cpVect gravity                  = cpv(0, -196);
        cpFloat marbleRadius_Range[2]   = {2, 4};
        int32_t marblesCount            = 0;
        int32_t marblesMax              = 200;
        double resetInS_default         = 15;
        double resetInS                 = 0;

        size_t numPegs      = 0;     // all pegs must be consecutive, starting at circles[0]
        size_t numCircles   = 0;  // pegs + circlular-marbles (with pegs first, then marbles)
        size_t numBoxes     = 0;
        size_t numBodies    = 0;
    } game;
    
    
    //
    // User Interface
    //
    std::bitset<128> keysPressed;  // key-press state: 0|false for up, 1|true for pressed-down; indexed by 7-bit ASCII codes
    bool showGUIDemo = false;
    bool showSettings = false;
    bool configurationMode = false;
    bool doEndConfiguration = false;

    //
    // Physics
    //
    cpSpace * physicsSpace = NULL;
    cpCircleShape circles[FSTUFF_MaxCircles] = {0};
    gbVec4 circleColors[FSTUFF_MaxCircles] = {0};
    cpSegmentShape boxes[FSTUFF_MaxBoxes] = {0};
    gbVec4 boxColors[FSTUFF_MaxBoxes] = {0};
    cpBody bodies[FSTUFF_MaxShapes] = {0};


    FSTUFF_Simulation();
    ~FSTUFF_Simulation();
    void    AddMarble();
    void    EventReceived(FSTUFF_Event * event);
    void    Render();
    void    Update();
    void    ViewChanged(const FSTUFF_ViewSize & viewSize);
    void    Init();
    bool    DidInit() const;
    void    ResetWorld();
    cpVect  globalScale = {1., 1.};
    void    SetGlobalScale(cpVect scale);
    cpFloat GetWorldWidth() const { return viewSize.widthMM * (1. / globalScale.x); }
    cpFloat GetWorldHeight() const { return viewSize.heightMM * (1. / globalScale.y); }
    void    UpdateProjectionMatrix();
    FSTUFF_CursorInfo cursorInfo;
    void    UpdateCursorInfo(const FSTUFF_CursorInfo & newInfo);
    
private:
    void    InitWorld();
    void    InitGPUShapes();
public: // public is needed, here, for FSTUFF_Shutdown
    void    ShutdownWorld();
    void    ShutdownGPU();

public:
    cpBody *          GetBody(size_t index)     { return &(this->bodies[index]); }
    cpCircleShape *   GetCircle(size_t index)   { return &(this->circles[index]); }
    cpSegmentShape *  GetBox(size_t index)      { return &(this->boxes[index]); }

    cpBody *          NewBody()     { return GetBody(this->game.numBodies++); }
    cpCircleShape *   NewCircle()   { return GetCircle(this->game.numCircles++); }
    cpSegmentShape *  NewBox()      { return GetBox(this->game.numBoxes++); }

    size_t IndexOfCircle(cpShape * shape)   { return ((((uintptr_t)(shape)) - ((uintptr_t)(&this->circles[0]))) / sizeof(this->circles[0])); }
    size_t IndexOfBox(cpShape * shape)      { return ((((uintptr_t)(shape)) - ((uintptr_t)(&this->boxes[0]))) / sizeof(this->boxes[0])); }
};


#define SPACE (this->physicsSpace)

template <typename FSTUFF_RendererType>
void FSTUFF_ShapeInit(FSTUFF_Shape * shape, FSTUFF_RendererType * renderer)
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


template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::InitGPUShapes()
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

    this->debugShape.debugName = "FSTUFF_DebugShape";
    this->debugShape.type = FSTUFF_ShapeDebug;
    this->debugShape.appearance = FSTUFF_ShapeAppearanceFilled;
    FSTUFF_ShapeInit(&(this->debugShape), this->renderer);
}

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::InitWorld()
{
    //
    // Physics-world init
    //
    memset(&circles, 0, sizeof(circles));
    memset(&circleColors, 0, sizeof(circleColors));
    memset(&boxes, 0, sizeof(boxes));
    memset(&boxColors, 0, sizeof(boxColors));
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
    shape = (cpShape*)cpSegmentShapeInit(NewBox(), body, cpv(wallLeft,wallBottom), cpv(wallRight,wallBottom), wallThickness/2.);
    cpSpaceAddShape(SPACE, shape);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    this->boxColors[IndexOfBox(shape)] = FSTUFF_Color(0x000000, 0x00);
    // Left
    shape = (cpShape*)cpSegmentShapeInit(NewBox(), body, cpv(wallLeft,wallBottom), cpv(wallLeft,wallTop), wallThickness/2.);
    cpSpaceAddShape(SPACE, shape);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    this->boxColors[IndexOfBox(shape)] = FSTUFF_Color(0x000000, 0x00);
    // Right
    shape = (cpShape*)cpSegmentShapeInit(NewBox(), body, cpv(wallRight,wallBottom), cpv(wallRight,wallTop), wallThickness/2.);
    cpSpaceAddShape(SPACE, shape);
    cpShapeSetElasticity(shape, 0.8);
    cpShapeSetFriction(shape, 1);
    this->boxColors[IndexOfBox(shape)] = FSTUFF_Color(0x000000, 0x00);
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
        const int which_peg_type = 0;
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
                w = this->GetWorldWidth() * 4.;
                h = this->GetWorldHeight() * 4.;
                angleRad = 0.;
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
                shape = (cpShape*)cpSegmentShapeInit(NewBox(), body, cpv(-w/2.,0.), cpv(w/2.,0.), h/2.);
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

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::AddMarble()
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

template <typename FSTUFF_RendererType>
bool FSTUFF_Simulation<FSTUFF_RendererType>::DidInit() const
{
    return (this->state != FSTUFF_DEAD);
}

template <typename FSTUFF_RendererType>
FSTUFF_Simulation<FSTUFF_RendererType>::FSTUFF_Simulation()
{
    FSTUFF_Log("%s, this:%p\n", __FUNCTION__, this);
}

template <typename FSTUFF_RendererType>
FSTUFF_Simulation<FSTUFF_RendererType>::~FSTUFF_Simulation()
{
    FSTUFF_Log("%s, this:%p, this->state:%d\n", __FUNCTION__, this, this->state);
}

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::Init() //, void * gpuDevice, void * nativeView)
{
    FSTUFF_Log("%s, this:%p, state:%d, renderer:%p\n",
        __FUNCTION__, this, this->state, this->renderer);

    // FSTUFF_FatalError("uh oh: %d", 123);
    
    if ( ! this->renderer) {
        FSTUFF_FatalError("FSTUFF_Simulation's 'renderer' field must be set, before calling its Init() method!");
    }
    
    // Don't re-initialize simulations that are already alive
    if (this->state != FSTUFF_DEAD) {
        return;
    }
    
    srand(time(nullptr));

    // Reset relevant variables (in 'this->game')
    game = FSTUFF_Simulation<FSTUFF_RendererType>::Resettable();

    // Mark simulation as alive
    this->state = FSTUFF_ALIVE;

    // Initialize 'this'
    this->keysPressed.reset();  // Mark all keys as being down/un-pressed
    FSTUFF_Assert(this->viewSize.widthPixels > 0);
    FSTUFF_Assert(this->viewSize.heightPixels > 0);
    this->InitGPUShapes();
    this->InitWorld();
}

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::ResetWorld()
{
    this->ShutdownWorld();
    this->game = FSTUFF_Simulation<FSTUFF_RendererType>::Resettable();
    this->InitWorld();
}

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::Update()
{
    ImGui::StyleColorsDark(&ImGui::GetStyle());

    if (this->showGUIDemo) {
        ImGui::ShowDemoWindow();
    }

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

    // Compute delta-time
    const double deltaTimeS = nowS - this->game.lastUpdateUTCTimeS;
    this->game.elapsedTimeS += deltaTimeS;
    
    // Add marbles, as warranted
    if (this->game.marblesCount < this->game.marblesMax) {
        if (this->game.addNumMarblesPerSecond > 0) {
            this->game.addMarblesInS -= deltaTimeS;
            if (this->game.addMarblesInS <= 0) {
                this->AddMarble();
                this->game.addMarblesInS = 1.f / this->game.addNumMarblesPerSecond;
            }
        }
    }

    // Update physics
    while ((this->game.lastUpdateUTCTimeS + kPhysicsStepTimeS) <= nowS) {
        cpSpaceStep(SPACE, kPhysicsStepTimeS);
        this->game.lastUpdateUTCTimeS += kPhysicsStepTimeS;
    }

    // Reset world, if warranted
    if (this->game.marblesCount >= this->game.marblesMax) {
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
        ImGui::SliderInt("Marbles, Max", &this->game.marblesMax, 0, 1000);
        if (ImGui::SliderFloat("Spawn Rate (marbles/second)", &this->game.addNumMarblesPerSecond, 0, 10, "%.3f", 3.0f)) {
            this->game.addMarblesInS = 1.f / this->game.addNumMarblesPerSecond;
        }
        ImGui::InvisibleButton("padding1", ImVec2(0, 8));
        ImGui::Separator();
        ImGui::InvisibleButton("padding2", ImVec2(0, 8));
        if (ImGui::Button("Restart Simulation", ImVec2(400, 32))) {
            this->ResetWorld();
        }
        if (this->configurationMode) {
            ImGui::InvisibleButton("padding1", ImVec2(0, 8));
            ImGui::Separator();
            ImGui::InvisibleButton("padding2", ImVec2(0, 8));
            if (ImGui::Button("OK", ImVec2(400, 32))) {
//                FSTUFF_Log("%s, doEndConfiguration set to true for sim:%p\n", __PRETTY_FUNCTION__, this);
                this->doEndConfiguration = true;
            }
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
        cpVect a = cpSegmentShapeGetA((cpShape*)GetBox(i));
        cpVect b = cpSegmentShapeGetB((cpShape*)GetBox(i));
        cpVect center = cpvlerp(a, b, 0.5);
        cpFloat radius = cpSegmentShapeGetRadius((cpShape*)GetBox(i));
        cpBody * body = cpShapeGetBody((cpShape*)GetBox(i));
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
        
        this->renderer->SetShapeProperties(FSTUFF_ShapeBox, i, dest, this->boxColors[i]);
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

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::Render()
{
    renderer->RenderShapes(&circleFilled, 0,            game.numCircles,                0.35f);
    renderer->RenderShapes(&circleDots,   game.numPegs, game.numCircles - game.numPegs, 1.0f);
    renderer->RenderShapes(&circleEdged,  0,            game.numCircles,                1.0f);
    renderer->RenderShapes(&boxFilled,    0,            game.numBoxes,                  0.35f);
    renderer->RenderShapes(&boxEdged,     0,            game.numBoxes,                  1.0f);

#if FSTUFF_USE_DEBUG_PEGS
    renderer->RenderShapes(&debugShape, 0, 1, 0.5678);
#endif

}

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::ViewChanged(const FSTUFF_ViewSize &viewSize)
{
    this->viewSize = viewSize;
    this->UpdateProjectionMatrix();
    FSTUFF_Assert(this->renderer);
    this->renderer->ViewChanged();
}

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::SetGlobalScale(cpVect scale)
{
    this->globalScale = scale;
    this->UpdateProjectionMatrix();
}

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::UpdateProjectionMatrix()
{
    FSTUFF_Log("%s: viewSize=%f,%f (mm); globalScale=%f,%f\n",
        __FUNCTION__,
        this->viewSize.widthMM, this->viewSize.heightMM,
        this->globalScale.x, this->globalScale.y
    );

    gbMat4 translation;
    gb_mat4_translate(&translation, {-1, -1, 0});
    
    gbMat4 scaling;
    gb_mat4_scale(&scaling, {
        (float)(2.0f / this->viewSize.widthMM),
        (float)(2.0f / this->viewSize.heightMM),
        1
    });

    gbMat4 scaling2;
    gb_mat4_identity(&scaling2);
    gb_mat4_scale(&scaling2, {
        (float)(this->globalScale.x),
        (float)(this->globalScale.y),
        1
    });

    this->projectionMatrix = (translation * scaling) * scaling2;
}

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::UpdateCursorInfo(const FSTUFF_CursorInfo &newInfo)
{
    const FSTUFF_CursorInfo oldInfo = cursorInfo;

    if (newInfo.pressed != oldInfo.pressed) {
        cursorInfo.pressed = newInfo.pressed;

        FSTUFF_Event event;
        memset(&event, 0, sizeof(event));
        event.type = FSTUFF_CursorButton;
        event.data.cursorButton.xOS = cursorInfo.xOS;
        event.data.cursorButton.yOS = cursorInfo.yOS;
        event.data.cursorButton.down = cursorInfo.pressed;
        this->EventReceived(&event);
    }

    if (newInfo.xOS != oldInfo.xOS || newInfo.yOS != oldInfo.yOS) {
        cursorInfo.xOS = newInfo.xOS;
        cursorInfo.yOS = newInfo.yOS;

        FSTUFF_Event event;
        memset(&event, 0, sizeof(event));
        event.type = FSTUFF_CursorMotion;
        event.data.cursorMotion.xOS = cursorInfo.xOS;
        event.data.cursorMotion.yOS = cursorInfo.yOS;
        this->EventReceived(&event);
    }
}

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::ShutdownWorld()
{
    for (size_t i = 0; i < this->game.numCircles; ++i) {
        cpShapeDestroy((cpShape*)GetCircle(i));
    }
    for (size_t i = 0; i < this->game.numBoxes; ++i) {
        cpShapeDestroy((cpShape*)GetBox(i));
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

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::ShutdownGPU()
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
}

template <typename FSTUFF_RendererType>
void FSTUFF_Simulation<FSTUFF_RendererType>::EventReceived(FSTUFF_Event *event)
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
            if (event->data.key.utf8[0] <= 127) {
                const auto key = event->data.key.utf8[0];
                this->keysPressed[key] = 1;
                guiIO.KeysDown[key] = 1;
                bool unhandled = false;
                switch (std::toupper(key)) {
                    case 'D': {
                        this->showGUIDemo = !this->showGUIDemo;
                    } break;
                    case 'R': {
                        this->ResetWorld();
                    } break;
                    case 'S': {
                        if ( ! this->configurationMode) {
                            this->showSettings = !this->showSettings;
                        }
                    } break;
                    default: {
                        unhandled = true;
                    } break;
                }
                if (!unhandled) {
                    event->handled = true;
                }
            }
        
        } break;
        
        case FSTUFF_EventKeyUp: {
            if (event->data.key.utf8[0] <= 127) {
                const auto key = event->data.key.utf8[0];
                this->keysPressed[key] = 0;
                guiIO.KeysDown[key] = 0;
            }
        } break;
        
        case FSTUFF_CursorButton: {
//            FSTUFF_Log("FSTUFF_CursorButton: os position = {%f,%f}, down?=%s\n", event->data.cursorButton.xOS, event->data.cursorButton.yOS, (event->data.cursorButton.down ? "YES" : "NO"));
        } break;

        case FSTUFF_CursorMotion: {
//            FSTUFF_Log("FSTUFF_CursorMotion: os position = {%f,%f}\n", event->data.cursorMotion.xOS, event->data.cursorMotion.yOS);
        } break;
        
        case FSTUFF_CursorContained: {
            FSTUFF_Log("FSTUFF_CursorContained: contained?=%s\n", (event->data.cursorContain.contained ? "YES" : "NO"));
        } break;
    }
}


// #endif /* FSTUFF_Simulation_hpp */
