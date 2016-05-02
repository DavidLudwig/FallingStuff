//
//  SharedStructures.h
//  MetalTest
//
//  Created by David Ludwig on 4/30/16.
//  Copyright (c) 2016 David Ludwig. All rights reserved.
//

#ifndef SharedStructures_h
#define SharedStructures_h

#include <simd/simd.h>

//typedef struct __attribute__((__aligned__(256)))
//{
//    matrix_float4x4 modelview_projection_matrix;
//    matrix_float4x4 normal_matrix;
//} uniforms_t;

typedef struct
{
    matrix_float4x4 modelview_projection_matrix;
    vector_float4 color;
} FSTUFF_ShapeInstance;

#endif /* SharedStructures_h */

