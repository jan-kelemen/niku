#ifndef HEATX_MATERIALS_INCLUDED
#define HEATX_MATERIALS_INCLUDED

layout(set = 1, binding = 0) uniform texture2D textures[];
layout(set = 1, binding = 1) uniform sampler samplers[];

struct Material
{
    uint baseColorTextureIndex;
    float alphaCutoff;
};

layout(std430, set = 1, binding = 2) readonly buffer MaterialBuffer
{
    Material v[];
} materials;

#endif