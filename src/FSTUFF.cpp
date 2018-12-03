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

#define GB_MATH_IMPLEMENTATION
#include "gb_math.h"

//#define FSTUFF_USE_DEBUG_PEGS 1



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


#pragma mark - Rendering

// FSTUFF_Renderer::~FSTUFF_Renderer()
// {
// }


#pragma mark - Shapes, Circle

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


#pragma mark - Simulation

static void FSTUFF_DefaultFatalErrorHandler(const char * formattedMessage, void *) {
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


//void FSTUFF_Shutdown(FSTUFF_Simulation * sim)
//{
//    sim->ShutdownWorld();
//    sim->ShutdownGPU();
//    sim->~FSTUFF_Simulation();
//    ::operator delete(sim);
//}


#pragma mark - Input Events

FSTUFF_Event FSTUFF_Event::NewKeyEvent(FSTUFF_EventType eventType, const char * utf8Char)
{
    FSTUFF_Event event;
    memset(&event, 0, sizeof(event));
    event.type = eventType;
    const size_t srcSize = strlen(utf8Char) + 1;
    const size_t copySize = std::min(sizeof(event.data.key.utf8), srcSize);
#if _MSC_VER
	strncpy_s(const_cast<char *>(event.data.key.utf8), copySize, utf8Char, _TRUNCATE);
	const_cast<char *>(event.data.key.utf8)[sizeof(event.data.key.utf8)-1] = '\0';
#else
    strlcpy(const_cast<char *>(event.data.key.utf8), utf8Char, copySize);
#endif
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

