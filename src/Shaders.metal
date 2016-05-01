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

struct vertex_t
{
    float4 position [[position]];
    float4 color;
};

vertex vertex_t vertex_circle_fill(constant float4 * position [[buffer(0)]],
                                   constant constants_t * constants [[buffer(1)]],
                                   uint vid [[vertex_id]],
                                   uint iid [[instance_id]])
{
    vertex_t vert;
    vert.position = constants[iid].modelview_projection_matrix * position[vid];
    vert.color = constants[iid].color_fill;
    return vert;
}

vertex vertex_t vertex_circle_edge(constant float4 * position [[buffer(0)]],
                                   constant constants_t * constants [[buffer(1)]],
                                   uint vid [[vertex_id]],
                                   uint iid [[instance_id]])
{
    vertex_t vert;
    vert.position = constants[iid].modelview_projection_matrix * position[vid];
    vert.color = constants[iid].color_edge;
    return vert;
}


fragment float4 fragment_main(vertex_t vert [[stage_in]])
{
    return vert.color;
}

