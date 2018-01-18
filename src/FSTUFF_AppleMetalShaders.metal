//
//  FSTUFF_AppleMetalShaders.metal
//  FallingStuff
//
//  Created by David Ludwig on 4/30/16.
//  Copyright (c) 2018 David Ludwig. All rights reserved.
//

#include <metal_stdlib>
using namespace metal;

#include "FSTUFF_AppleMetalStructs.h"

struct FSTUFF_Vertex
{
    float4 position [[position]];
    float4 color;
};

vertex FSTUFF_Vertex FSTUFF_VertexShader(constant float4 * position [[buffer(0)]],
                                         constant FSTUFF_GPUGlobals * gpuGlobals [[buffer(1)]],
                                         constant FSTUFF_ShapeGPUInfo * gpuShapes [[buffer(2)]],
                                         constant float * alpha [[buffer(3)]],
                                         uint vertexId [[vertex_id]],
                                         uint shapeId [[instance_id]])
{
    FSTUFF_Vertex vert;
    vert.position = (gpuGlobals->projection_matrix * gpuShapes[shapeId].model_matrix) * position[vertexId];
    vert.color = {
        gpuShapes[shapeId].color[0],
        gpuShapes[shapeId].color[1],
        gpuShapes[shapeId].color[2],
        gpuShapes[shapeId].color[3] * (*alpha),
    };
    return vert;
}

fragment float4 FSTUFF_FragmentShader(FSTUFF_Vertex vert [[stage_in]])
{
    return vert.color;
}

