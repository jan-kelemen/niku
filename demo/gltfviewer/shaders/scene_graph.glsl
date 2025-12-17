#ifndef GLTFVIEWER_SCENE_GRAPH_INCLUDED
#define GLTFVIEWER_SCENE_GRAPH_INCLUDED

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

struct PackedVertex
{
    vec4 position_uv_s;
    vec4 normal_uv_t;
    vec4 tangent;
    vec4 color;
};

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec4 color;
    vec2 uv;
};

layout(buffer_reference, buffer_reference_align = 64) readonly buffer VertexBuffer
{
    PackedVertex v[];
};

Vertex unpackVertex(PackedVertex vtx)
{
    return Vertex(vtx.position_uv_s.xyz, vtx.normal_uv_t.xyz, vtx.tangent, vtx.color, vec2(vtx.position_uv_s.w, vtx.normal_uv_t.w));
}

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
