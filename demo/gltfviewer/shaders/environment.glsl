#ifndef GLTFVIEWER_ENVIRONMENT_INCLUDED
#define GLTFVIEWER_ENVIRONMENT_INCLUDED

struct Light
{
    vec3 position;
    uint type;
    vec4 color;
    mat4 lightSpace;
};

layout(std430, set = 0, binding = 0) readonly buffer Environment
{
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
    uint prefilteredMipLevels;
    uint lightCount;
    Light lights[];
} env;

layout(set = 0, binding = 1) uniform samplerCube irradianceMap;
layout(set = 0, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 0, binding = 3) uniform sampler2D samplerBRDF;

#endif
