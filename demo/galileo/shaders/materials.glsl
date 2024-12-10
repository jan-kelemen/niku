#ifndef GALILEO_MATERIALS_INCLUDED
#define GALILEO_MATERIALS_INCLUDED

layout(set = 1, binding = 0) uniform texture2D textures[];
layout(set = 1, binding = 1) uniform sampler samplers[];

struct Material
{
    vec4 baseColorFactor;
    vec3 emissiveFactor;
    uint baseColorTextureIndex;
    uint baseColorSamplerIndex;
    float alphaCutoff;
    uint metallicRoughnessTextureIndex;
    uint metallicRoughnessSamplerIndex;
    float metallicFactor;
    float roughnessFactor;
    float occlusionStrength;
    uint normalTextureIndex;
    uint normalSamplerIndex;
    uint emissiveTextureIndex;
    uint emissiveSamplerIndex;
    uint occlusionTextureIndex;
    uint occlusionSamplerIndex;
    float normalScale;
    uint doubleSided;
    float emissiveStrength;
};

layout(std430, set = 1, binding = 2) readonly buffer MaterialBuffer
{
    Material v[];
} materials;

#endif
