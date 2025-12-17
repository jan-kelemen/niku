#ifndef GLTFVIEWER_SCENE_GRAPH_INCLUDED
#define GLTFVIEWER_SCENE_GRAPH_INCLUDED

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

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

layout(buffer_reference, buffer_reference_align = 128) readonly buffer TransformBuffer
{
    Transform v[];
};

#endif
