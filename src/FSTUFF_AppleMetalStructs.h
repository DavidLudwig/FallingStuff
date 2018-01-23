//
//  FSTUFF_AppleMetalStructs.h
//  MetalTest
//
//  Created by David Ludwig on 4/30/16.
//  Copyright (c) 2018 David Ludwig. All rights reserved.
//

#ifndef FSTUFF_AppleMetalStructs_h
#define FSTUFF_AppleMetalStructs_h

#include <simd/simd.h>
#include "FSTUFF_Constants.h"

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
    FSTUFF_ShapeGPUInfo circles[FSTUFF_MaxCircles];
    FSTUFF_ShapeGPUInfo boxes[FSTUFF_MaxBoxes];
} FSTUFF_GPUData;

typedef struct {
    vector_float4 position;
    vector_float4 color;
} VertexInfo;

#endif /* FSTUFF_AppleMetalStructs_h */
