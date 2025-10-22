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
#include <cstdint>  // C++ std library, fixed-width integer types
#include <random>   // C++ std library, random numbers
#include <tuple>    // C++ std library, tuples
#include <chipmunk/chipmunk.h>  // Physics library
extern "C" {
    //#include <chipmunk/chipmunk_structs.h>
    #include <chipmunk/chipmunk_private.h>  // #include'd for allowing static cp* structs (cpSpace, cpBody, etc.)
}
#include "gb_math.h"            // Vector and Matrix math
#include "imgui.h"

#ifndef FSTUFF_ENABLE_IMGUI_DEMO
    // If 1, allow the ImGUI demo window to be shown.  This can increase
    // compiled app-size by over 100 KB (measured with Emscripten)
    #if defined(NDEBUG)
        #define FSTUFF_ENABLE_IMGUI_DEMO 0
    #else
        #define FSTUFF_ENABLE_IMGUI_DEMO 1
    #endif
#endif

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
void FSTUFF_DefaultFatalErrorHandler(const char * formattedMessage, void *);

#define FSTUFF_FatalError(...) FSTUFF_FatalError_Inner(FSTUFF_CODELOC, __VA_ARGS__)

#define FSTUFF_Assert(CONDITION) if (!(CONDITION)) { FSTUFF_FatalError("Assertion failed: %s", #CONDITION); }
#define FSTUFF_AssertUnimplemented() FSTUFF_FatalError("UNIMPLEMENTED CODE!")


#if 0
    #define FSTUFF_LOG_IMPLEMENT_ME(EXTRA) FSTUFF_Log("%s: %s%s\n", "IMPLEMENT ME", __FUNCTION__, EXTRA)
#else
    #define FSTUFF_LOG_IMPLEMENT_ME(EXTRA)
#endif

void FSTUFF_OpenWebPage(const char * url);

enum FSTUFF_ShapeType : uint8_t {
    FSTUFF_ShapeCircle = 0,
    FSTUFF_ShapeBox,
    FSTUFF_ShapeSegment,
    FSTUFF_ShapeDebug,
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

struct FSTUFF_Simulation;

typedef void * FSTUFF_Texture;

struct FSTUFF_Renderer {
    FSTUFF_Simulation * sim = nullptr;

    virtual         ~FSTUFF_Renderer();
    
    virtual void    BeginFrame() = 0;

    virtual void *  NewVertexBuffer(void * src, size_t size) = 0;
    virtual void    DestroyVertexBuffer(void * gpuVertexBuffer) = 0;

    virtual FSTUFF_Texture NewTexture(const uint8_t * srcRGBA32, int width, int height) = 0;
    virtual void    DestroyTexture(FSTUFF_Texture tex) = 0;

    virtual void    ViewChanged() = 0;
    virtual void    RenderShapes(FSTUFF_Shape * shape, size_t offset, size_t count, float alpha) = 0;
    virtual void    SetProjectionMatrix(const gbMat4 & matrix) = 0;
    virtual void    SetShapeProperties(FSTUFF_ShapeType shape, size_t i, const gbMat4 & matrix, const gbVec4 & color) = 0;
    virtual FSTUFF_CursorInfo GetCursorInfo() = 0;
};

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
            char32_t utf32;
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
    static FSTUFF_Event NewKeyEvent(FSTUFF_EventType keyEventType, char32_t utf32Char);
//    static FSTUFF_Event NewCursorButtonEvent(float xOS, float yOS, bool down);
//    static FSTUFF_Event NewCursorMotionEvent(float xOS, float yOS);
//    static FSTUFF_Event NewCursorContainedEvent(bool contained);

    std::string KeyToUTF8() const;
    char32_t KeyToUTF32() const { return this->data.key.utf32; }
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
    FSTUFF_Shape segmentFilled;
    FSTUFF_Shape segmentEdged;
    FSTUFF_Shape debugShape;
    gbMat4 projectionMatrix;
    //cpVect viewSizeMM = {0, 0};
    FSTUFF_ViewSize viewSize;
    FSTUFF_Renderer * renderer = NULL;
    
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
        // Display
        //
        gbVec3 viewTranslation = {0., 0., 0.};

        //
        // Misc parameters
        //
        float addMarblesInS             = 0.0f;
        cpVect gravity                  = cpv(0, -196);
        cpFloat marbleRadius_Range[2]   = {2, 4};
        int32_t marblesCount            = 0;
        double resetInS_default         = 15;
        double resetInS                 = 0;

        size_t numPegs      = 0;     // all pegs must be consecutive, starting at circles[0]
        size_t numCircles   = 0;  // pegs + circlular-marbles (with pegs first, then marbles)
        size_t numBoxes     = 0;
        size_t numSegments  = 0;
        size_t numBodies    = 0;
    } game;

    //
    // User-adjustable parameters
    //
    float addNumMarblesPerSecond = 1.0f;
    int32_t marblesMax = 200;

    //
    // User Interface
    //
    ImGuiContext * imGuiContext = nullptr;
    std::bitset<128> keysPressed;       // key-press state: 0|false for up, 1|true for pressed-down; indexed by 7-bit ASCII codes
    bool showGUIDemo = false;           // only works if '#define FSTUFF_ENABLE_IMGUI_DEMO 1' is set (increases app-size!)
    bool showSettings = false;
    bool configurationMode = false;
    bool doEndConfiguration = false;

    //
    // Physics
    //
    cpSpace * physicsSpace = NULL;
    cpCircleShape circles[FSTUFF_MaxCircles] = {0};
    gbVec4 circleColors[FSTUFF_MaxCircles] = {0};
    cpPolyShape boxes[FSTUFF_MaxBoxes] = {0};
    gbVec4 boxColors[FSTUFF_MaxBoxes] = {0};
    cpSegmentShape segments[FSTUFF_MaxSegments] = {0};
    gbVec4 segmentColors[FSTUFF_MaxSegments] = {0};
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
    cpPolyShape *     GetBox(size_t index)      { return &(this->boxes[index]); }
    cpSegmentShape *  GetSegment(size_t index)  { return &(this->segments[index]); }

    cpBody *          NewBody()     { return GetBody(this->game.numBodies++); }
    cpCircleShape *   NewCircle()   { return GetCircle(this->game.numCircles++); }
    cpPolyShape *     NewBox()      { return GetBox(this->game.numBoxes++); }
    cpSegmentShape *  NewSegment()  { return GetSegment(this->game.numSegments++); }

    size_t IndexOfCircle(cpShape * shape)   { return ((((uintptr_t)(shape)) - ((uintptr_t)(&this->circles[0]))) / sizeof(this->circles[0])); }
    size_t IndexOfBox(cpShape * shape)      { return ((((uintptr_t)(shape)) - ((uintptr_t)(&this->boxes[0]))) / sizeof(this->boxes[0])); }
    size_t IndexOfSegment(cpShape * shape)  { return ((((uintptr_t)(shape)) - ((uintptr_t)(&this->segments[0]))) / sizeof(this->segments[0])); }
};

#endif /* FSTUFF_Simulation_hpp */
