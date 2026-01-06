#ifndef HEATX_GEOMETRY_INCLUDED
#define HEATX_GEOMETRY_INCLUDED

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

layout(buffer_reference, buffer_reference_align = 4) readonly buffer IndexBuffer
{
    uint v[];
};

struct Primitive 
{
    VertexBuffer vertices;
    IndexBuffer indices;
    uint count;
    uint material_index;
    uint padding[2];
};

layout(buffer_reference, buffer_reference_align = 32) readonly buffer PrimitiveBuffer
{
    Primitive v[];
};

struct Triangle
{
    Vertex vertices[3];
    vec3 normal;
    vec2 uv;
    uint material_index;
};

Triangle unpackTriangle(PrimitiveBuffer primitives, const uint geometry_index, const uint primitive_index, const vec2 intersection) {
    Triangle rv;

    Primitive primitive = primitives.v[geometry_index];
    rv.material_index = primitive.material_index;

    const uint triangle_index = primitive_index * 3;

    for (uint i = 0; i < 3; i++)
    {
        rv.vertices[i] = unpackVertex(primitive.vertices.v[primitive.indices.v[triangle_index + i]]);
    }

    // Calculate values at barycentric coordinates
    vec3 barycentricCoords = vec3(1.0f - intersection.x - intersection.y, intersection.x, intersection.y);
    rv.uv = rv.vertices[0].uv * barycentricCoords.x + rv.vertices[1].uv * barycentricCoords.y + rv.vertices[2].uv * barycentricCoords.z;
    rv.normal = rv.vertices[0].normal * barycentricCoords.x + rv.vertices[1].normal * barycentricCoords.y + rv.vertices[2].normal * barycentricCoords.z;

    return rv;
}

#endif