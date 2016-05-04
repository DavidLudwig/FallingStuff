//
//  Shaders.metal
//  MetalTest
//
//  Created by David Ludwig on 4/30/16.
//  Copyright (c) 2016 David Ludwig. All rights reserved.
//

#include <metal_stdlib>
#include <simd/simd.h>
#include "SharedStructures.h"

using namespace metal;

struct FSTUFF_Vertex
{
    float4 position [[position]];
    float4 color;
};

vertex FSTUFF_Vertex FSTUFF_VertexShader(constant float4 * position [[buffer(0)]],
                                         constant FSTUFF_SimulationGPUInfo * gpuGlobals [[buffer(1)]],
                                         constant FSTUFF_ShapeGPUInfo * gpuShapes [[buffer(2)]],
                                         constant vector_float4 * color [[buffer(3)]],
                                         uint vertexId [[vertex_id]],
                                         uint shapeId [[instance_id]])
{
    FSTUFF_Vertex vert;
    vert.position = (gpuGlobals->projection_matrix * gpuShapes[shapeId].model_matrix) * position[vertexId];
    vert.color = *color;
    return vert;
}

fragment float4 FSTUFF_FragmentShader(FSTUFF_Vertex vert [[stage_in]])
{
    return vert.color;
}

