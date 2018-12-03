//
//  FSTUFF.h
//  FallingStuff
//
//  Created by David Ludwig on 5/30/16.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#ifndef FSTUFF_Simulation_hpp
#define FSTUFF_Simulation_hpp

#define _USE_MATH_DEFINES
#include <cmath>

#include <array>    // C++ std library, fixed-size arrays
#include <bitset>	// C++ std library, bit-sets
#include <random>   // C++ std library, random numbers
#include <chipmunk/chipmunk.h>  // Physics library
extern "C" {
    //#include <chipmunk/chipmunk_structs.h>
    #include <chipmunk/chipmunk_private.h>  // #include'd for allowing static cp* structs (cpSpace, cpBody, etc.)
}
#include "gb_math.h"            // Vector and Matrix math
#include "imgui.h"

#ifndef FSTUFF_USE_METAL
    #if __APPLE__
        #define FSTUFF_USE_METAL 1
    #else
        #define FSTUFF_USE_METAL 0
    #endif
#endif

#include "FSTUFF_Constants.h"   // Miscellaneous constants

#define FSTUFF_countof(arr) (sizeof(arr) / sizeof(arr[0]))

#if __GNUC__ || __clang__
    #define FSTUFF_FormatGuard(FUNCTION_TYPE, STRING_INDEX, FIRST_TO_CHECK) __attribute__((format(FUNCTION_TYPE, STRING_INDEX, FIRST_TO_CHECK)))
#else
    #define FSTUFF_FormatGuard(FUNCTION_TYPE, STRING_INDEX, FIRST_TO_CHECK)
#endif

void FSTUFF_Log(const char * fmt, ...) FSTUFF_FormatGuard(printf, 1, 2);

#if _MSC_VER
	#define FSTUFF_CurrentFunction __FUNCSIG__
#elif __GCC__ || __clang__
	#define FSTUFF_CurrentFunction __PRETTY_FUNCTION__
#else
	#define FSTUFF_CurrentFunction __FUNCTION__
#endif

struct FSTUFF_CodeLocation {
    const char * functionName;
    const char * fileName;
    int line;
};


#define FSTUFF_CODELOC { FSTUFF_CurrentFunction, __FILE__, __LINE__ }

void FSTUFF_FatalError_Inner(FSTUFF_CodeLocation location, const char *fmt, ...) FSTUFF_FormatGuard(printf, 2, 3);

#define FSTUFF_FatalError(...) FSTUFF_FatalError_Inner(FSTUFF_CODELOC, __VA_ARGS__)

#define FSTUFF_Assert(CONDITION) if (!(CONDITION)) { FSTUFF_FatalError("Assertion failed: %s", #CONDITION); }


#if 0
    #define FSTUFF_LOG_IMPLEMENT_ME(EXTRA) FSTUFF_Log("%s: %s%s\n", "IMPLEMENT ME", __FUNCTION__, EXTRA)
#else
    #define FSTUFF_LOG_IMPLEMENT_ME(EXTRA)
#endif

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

static const unsigned kNumCircleParts = 64; //32;

struct FSTUFF_ViewSize {
    float widthMM       = 0.f;
    float heightMM      = 0.f;
    int widthPixels     = 0;
    int heightPixels    = 0;
    int widthOS         = 0;
    int heightOS        = 0;
};

struct FSTUFF_CursorInfo {
    float xOS = -1.f;
    float yOS = -1.f;
    bool pressed = false;
    //bool contained;
};

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

void FSTUFF_MakeCircleLineStrip(gbVec4 * vertices, int maxVertices, int * numVertices, int numPartsToGenerate,
                                float radius);
void FSTUFF_MakeCircleTriangleStrip(gbVec4 * vertices, int maxVertices, int * numVertices, int numPartsToGenerate,
                                    float innerRadius, float outerRadius);
void FSTUFF_MakeCircleFilledTriangles(gbVec4 * vertices,
                                      int maxVertices,
                                      int * numVertices,
                                      int numPartsToGenerate,
                                      float radius,
                                      float offsetX,
                                      float offsetY);

int FSTUFF_RandRangeI(std::mt19937 & rng, int a, int b);
cpFloat FSTUFF_RandRangeF(std::mt19937 & rng, cpFloat a, cpFloat b);


// struct FSTUFF_Simulation;

#if 0
    #define FSTUFF_VIRTUAL virtual
    #define FSTUFF_OVERRIDE override
    #define FSTUFF_PURE_VIRTUAL = 0
#else
    #define FSTUFF_VIRTUAL
    #define FSTUFF_OVERRIDE
    #define FSTUFF_PURE_VIRTUAL
#endif

enum FSTUFF_EventType : uint8_t {
    FSTUFF_EventNone = 0,
    FSTUFF_EventKeyDown,
    FSTUFF_EventKeyUp,
    FSTUFF_CursorMotion,
    FSTUFF_CursorButton,
    FSTUFF_CursorContained,
};

struct FSTUFF_Event {
    FSTUFF_EventType type = FSTUFF_EventNone;
    bool handled = false;
    union FSTUFF_EventData {
        FSTUFF_EventData() { memset( this, 0, sizeof(FSTUFF_EventData) ); }
        struct {
            const char utf8[8];
        } key;
        struct {
            float xOS;
            float yOS;
            bool down;
        } cursorButton;
        struct {
            float xOS;
            float yOS;
        } cursorMotion;
        struct {
            bool contained;
        } cursorContain;
    } data;
    
    static FSTUFF_Event NewKeyEvent(FSTUFF_EventType keyEventType, const char * utf8Char);
//    static FSTUFF_Event NewCursorButtonEvent(float xOS, float yOS, bool down);
//    static FSTUFF_Event NewCursorMotionEvent(float xOS, float yOS);
//    static FSTUFF_Event NewCursorContainedEvent(bool contained);
};

#include "FSTUFF_Simulation.h"

template <typename FSTUFF_RendererType>
struct FSTUFF_Renderer {
    FSTUFF_Simulation<FSTUFF_RendererType> * sim = nullptr;

    FSTUFF_VIRTUAL         ~FSTUFF_Renderer();
    FSTUFF_VIRTUAL void    DestroyVertexBuffer(void * gpuVertexBuffer) FSTUFF_PURE_VIRTUAL;
    FSTUFF_VIRTUAL void *  NewVertexBuffer(void * src, size_t size) FSTUFF_PURE_VIRTUAL;
    FSTUFF_VIRTUAL void    ViewChanged() FSTUFF_PURE_VIRTUAL;
    FSTUFF_VIRTUAL void    RenderShapes(FSTUFF_Shape * shape, size_t offset, size_t count, float alpha) FSTUFF_PURE_VIRTUAL;
    FSTUFF_VIRTUAL void    SetProjectionMatrix(const gbMat4 & matrix) FSTUFF_PURE_VIRTUAL;
    FSTUFF_VIRTUAL void    SetShapeProperties(FSTUFF_ShapeType shape, size_t i, const gbMat4 & matrix, const gbVec4 & color) FSTUFF_PURE_VIRTUAL;
    FSTUFF_VIRTUAL FSTUFF_CursorInfo GetCursorInfo() FSTUFF_PURE_VIRTUAL;
};

#endif /* FSTUFF_Simulation_hpp */
