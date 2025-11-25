#ifndef GLTFVIEWER_SCENE_GRAPH_INCLUDED
#define GLTFVIEWER_SCENE_GRAPH_INCLUDED

#extension GL_EXT_scalar_block_layout : require

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec4 color;
    vec2 uv;
};

layout(scalar, set = 2, binding = 0) readonly buffer VertexBuffer
{
    Vertex v[];
} vertices;

struct Transform
{
    mat4 model;
    mat4 normal;
};

layout(std140, set = 2, binding = 1) readonly buffer TransformBuffer
{
    Transform v[];
} transforms;

#endif
