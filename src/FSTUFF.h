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
#include "FSTUFF_AppleMetalStructs.h"   // Apple-Metal structs
#include "FSTUFF_Colors.h"      // Platform-independent color definitions
#include "FSTUFF_Version.h"     // Version constants (of Falling Stuff)

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
    void * device = NULL;
    void * renderer = NULL;
    FSTUFF_GPUData * appData = NULL;      // on Apple+Metal, this is laid out as a 'FSTUFF_GPUData' struct
    void * nativeView = NULL;

    void   DestroyVertexBuffer(void * gpuVertexBuffer);
    void * NewVertexBuffer(void * src, size_t size);
    void   GetViewSizeMM(float *outWidthMM, float *outHeightMM);
    void   RenderShapes(FSTUFF_Shape * shape,
                        size_t offset,
                        size_t count,
                        float alpha);
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
    const   cpVect gravity                  = cpv(0, -196);
    const   cpFloat marbleRadius_Range[2]   = {2, 4};
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
        cpCircleShape circles[kMaxCircles] = {0};
        vector_float4 circleColors[kMaxCircles] = {0};
        
        size_t numBoxes = 0;
        cpSegmentShape boxes[kMaxBoxes] = {0};
        vector_float4 boxColors[kMaxBoxes] = {0};
        
        size_t numBodies = 0;
        cpBody bodies[kMaxShapes] = {0};
    } world;
    
    void    AddMarble();
    void    EventReceived(FSTUFF_Event * event);
    void    Render();
    void    Update();
    void    ViewChanged(float widthMM, float heightMM);

};

void         FSTUFF_Init(FSTUFF_Simulation * sim);
void         FSTUFF_Log(const char * fmt, ...) __attribute__((format(printf, 1, 2)));
FSTUFF_Event FSTUFF_NewKeyEvent(FSTUFF_EventType keyEventType, const char * utf8Char);

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#include <simd/simd.h>
void         FSTUFF_CopyMatrix(gbMat4 & dest, const matrix_float4x4 & src);
void         FSTUFF_CopyMatrix(matrix_float4x4 & dest, const gbMat4 & src);
void         FSTUFF_Log(NSString * fmt, ...) __attribute__((format(NSString, 1, 2)));
#endif

#endif /* FSTUFF_Simulation_hpp */
