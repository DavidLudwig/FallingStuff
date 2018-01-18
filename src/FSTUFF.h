//
//  FSTUFF.h
//  FallingStuff
//
//  Created by David Ludwig on 5/30/16.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#ifndef FSTUFF_Simulation_hpp
#define FSTUFF_Simulation_hpp

#include <random>   // C++ std library, random numbers

extern "C" {
    //#include <chipmunk/chipmunk_structs.h>
    #include <chipmunk/chipmunk_private.h>  // #include'd for allowing static cp* structs (cpSpace, cpBody, etc.)
}
#include <chipmunk/chipmunk.h>  // Physics library
#include "gb_math.h"            // Vector and Matrix math
#include "FSTUFF_Constants.h"   // Miscellaneous constants

void FSTUFF_Log(const char * fmt, ...) __attribute__((format(printf, 1, 2)));

enum FSTUFF_ShapeType : uint8_t {
    FSTUFF_ShapeCircle = 0,
    FSTUFF_ShapeBox,
    FSTUFF_ShapeDebug
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

enum FSTUFF_SimulationState : uint8_t {
    FSTUFF_DEAD = 0,
    FSTUFF_ALIVE
};

struct FSTUFF_Shape {
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
    void * gpuVertexBuffer = NULL;
};

struct FSTUFF_Renderer {
    virtual         ~FSTUFF_Renderer();
    virtual void    DestroyVertexBuffer(void * gpuVertexBuffer) = 0;
    virtual void *  NewVertexBuffer(void * src, size_t size) = 0;
    virtual void    GetViewSizeMM(float *outWidthMM, float *outHeightMM) = 0;
    virtual void    RenderShapes(FSTUFF_Shape * shape, size_t offset, size_t count, float alpha) = 0;
    virtual void    SetProjectionMatrix(const gbMat4 & matrix) = 0;
    virtual void    SetShapeProperties(FSTUFF_ShapeType shape, size_t i, const gbMat4 & matrix, const gbVec4 & color) = 0;
};

enum FSTUFF_EventType : uint8_t {
    FSTUFF_EventNone = 0,
    FSTUFF_EventKeyDown = 1,
};

struct FSTUFF_Event {
    FSTUFF_EventType type = FSTUFF_EventNone;
    bool handled = false;
    union FSTUFF_EventData {
        FSTUFF_EventData() { memset( this, 0, sizeof(FSTUFF_EventData) ); }
        struct {
            const char utf8[8];
        } key;
    } data;
    
    static FSTUFF_Event NewKeyEvent(FSTUFF_EventType keyEventType, const char * utf8Char);
};

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
    cpVect viewSizeMM = {0, 0};
    FSTUFF_Renderer * renderer = NULL;
    
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
    double addMarblesInS_Range[2]   = {0.3, 1.0};
    double addMarblesInS            = addMarblesInS_Range[0];
    cpVect gravity                  = cpv(0, -196);
    cpFloat marbleRadius_Range[2]   = {2, 4};
    int32_t marblesCount            = 0;
    int32_t marblesMax              = 200;
    double resetInS_default         = 15;
    double resetInS                 = 0;
    

    //
    // Physics
    //
    // All members here will get their memory zero'ed out on world-init.
    //
    struct World {
        // HACK: C++ won't allow us to create a 'cpSpace' directly, due to constructor issues,
        //   so we'll allocate one ourselves.
        //
        //   To note, Chipmunk Physics' cpAlloc() function just calls calloc() anyways,
        //   and according to its docs, using custom allocation, as is done here, is ok.
        //
        //uint8_t _physicsSpaceStorage[sizeof(cpSpace)];
        cpSpace * physicsSpace = NULL;
        
        size_t numPegs = 0;     // all pegs must be consecutive, starting at circles[0]
        size_t numCircles = 0;  // pegs + circlular-marbles (with pegs first, then marbles)
        cpCircleShape circles[FSTUFF_MaxCircles] = {0};
        gbVec4 circleColors[FSTUFF_MaxCircles] = {0};
        
        size_t numBoxes = 0;
        cpSegmentShape boxes[FSTUFF_MaxBoxes] = {0};
        gbVec4 boxColors[FSTUFF_MaxBoxes] = {0};
        
        size_t numBodies = 0;
        cpBody bodies[FSTUFF_MaxShapes] = {0};
    } world;
    
    void    AddMarble();
    void    EventReceived(FSTUFF_Event * event);
    void    Render();
    void    Update();
    void    ViewChanged(float widthMM, float heightMM);
    void    Init();
    void    ResetWorld();
    cpVect  globalScale = {1., 1.};
    void    SetGlobalScale(cpVect scale);
    cpFloat GetWorldWidth() const { return viewSizeMM.x * (1. / globalScale.x); }
    cpFloat GetWorldHeight() const { return viewSizeMM.y * (1. / globalScale.y); }
    void    UpdateProjectionMatrix();
    
private:
    void    InitWorld();
    void    InitGPUShapes();
public: // public is needed, here, for FSTUFF_Shutdown
    void    ShutdownWorld();
    void    ShutdownGPU();

public:
    cpBody *          GetBody(size_t index)     { return &(this->world.bodies[index]); }
    cpCircleShape *   GetCircle(size_t index)   { return &(this->world.circles[index]); }
    cpSegmentShape *  GetBox(size_t index)      { return &(this->world.boxes[index]); }

    cpBody *          NewBody()     { return GetBody(this->world.numBodies++); }
    cpCircleShape *   NewCircle()   { return GetCircle(this->world.numCircles++); }
    cpSegmentShape *  NewBox()      { return GetBox(this->world.numBoxes++); }

    size_t IndexOfCircle(cpShape * shape)   { return ((((uintptr_t)(shape)) - ((uintptr_t)(&this->world.circles[0]))) / sizeof(this->world.circles[0])); }
    size_t IndexOfBox(cpShape * shape)      { return ((((uintptr_t)(shape)) - ((uintptr_t)(&this->world.boxes[0]))) / sizeof(this->world.boxes[0])); }
};

#endif /* FSTUFF_Simulation_hpp */
