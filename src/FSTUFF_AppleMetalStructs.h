//
//  FSTUFF_AppleMetalStructs.h
//  MetalTest
//
//  Created by David Ludwig on 4/30/16.
//  Copyright (c) 2018 David Ludwig. All rights reserved.
//

#ifndef SharedStructures_h
#define SharedStructures_h

#include <simd/simd.h>
//#include "gb_math.h"

#define kMaxCircles 2048
#define kMaxBoxes 2048
#define kMaxShapes (kMaxCircles + kMaxBoxes)

typedef struct __attribute__((__aligned__(256)))
{
    matrix_float4x4 projection_matrix;
} FSTUFF_GPUGlobals;

typedef struct
{
    matrix_float4x4 model_matrix;
    vector_float4 color;
} FSTUFF_ShapeGPUInfo;

typedef struct
{
    FSTUFF_GPUGlobals globals;
    FSTUFF_ShapeGPUInfo circles[kMaxCircles];
    FSTUFF_ShapeGPUInfo boxes[kMaxBoxes];
} FSTUFF_GPUData;

#endif /* SharedStructures_h */