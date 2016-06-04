//
//  FSTUFF_Simulation.hpp
//  FallingStuff
//
//  Created by David Ludwig on 5/30/16.
//  Copyright © 2016 David Ludwig. All rights reserved.
//

#ifndef FSTUFF_Simulation_hpp
#define FSTUFF_Simulation_hpp

extern "C" {
    //#include <chipmunk/chipmunk_structs.h>
    #include <chipmunk/chipmunk_private.h>  // #include'd solely for allowing static cp* structs (cpSpace, cpBody, etc.)
}
#include <chipmunk/chipmunk.h>

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
    void * gpuVertexBuffer = NULL;
};

struct FSTUFF_Simulation {
    //
    // Geometry + GPU
    //
    FSTUFF_ShapeTemplate circleFilled;
    FSTUFF_ShapeTemplate circleDots;
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
    // All members here will get their memory zero'ed out on world-init.
    //
    struct World {
        // HACK: C++ won't allow us to create a 'cpSpace' directly, due to constructor issues,
        //   so we'll allocate one ourselves.
        //
        //   To note, Chipmunk Physics' cpAlloc() function just calls calloc() anyways,
        //   and according to its docs, using custom allocation, as is done here, is ok.
        //
        uint8_t _physicsSpaceStorage[sizeof(cpSpace)];
        cpSpace * physicsSpace;
        
        size_t numPegs;     // all pegs must be consecutive, starting at circles[0]
        size_t numCircles;  // pegs + circlular-marbles (with pegs first, then marbles)
        cpCircleShape circles[kMaxCircles];
        vector_float4 circleColors[kMaxCircles];
        
        size_t numBoxes;
        cpSegmentShape boxes[kMaxBoxes];
        vector_float4 boxColors[kMaxBoxes];
        
        size_t numBodies;
        cpBody bodies[kMaxShapes];
    } world;
};

enum FSTUFF_EventType : uint8_t {
    FSTUFF_EventKeyDown = 1,
};

struct FSTUFF_Event {
    FSTUFF_EventType type;
    union {
        struct {
            const char * utf8;
        } key;
    };
};


void FSTUFF_SimulationInit(FSTUFF_Simulation * sim, void * gpuDevice);
void FSTUFF_SimulationEvent(FSTUFF_Simulation * sim, FSTUFF_Event * event);
void FSTUFF_SimulationUpdate(FSTUFF_Simulation * sim, FSTUFF_GPUData * gpuData);
void FSTUFF_SimulationRender(FSTUFF_Simulation * sim,
                             void * gpuDevice,
                             void * gpuRenderer,
                             void * gpuAppData);
void FSTUFF_SimulationViewChanged(FSTUFF_Simulation * sim, float widthMM, float heightMM);


#endif /* FSTUFF_Simulation_hpp */
