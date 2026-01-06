#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec2 attribs;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 2) uniform CameraProperties 
{
    mat4 viewInverse;
    mat4 projInverse;
    vec4 lightPosition;
} cam;

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

layout(push_constant) uniform PushConsts
{
    PrimitiveBuffer primitives;
} pc;

Triangle unpackTriangle(uint geometry_index, uint primitive_index) {
    Triangle rv;

    Primitive primitive = pc.primitives.v[geometry_index];
    rv.material_index = primitive.material_index;

    const uint triangle_index = primitive_index * 3;

    for (uint i = 0; i < 3; i++)
    {
        rv.vertices[i] = unpackVertex(primitive.vertices.v[primitive.indices.v[triangle_index + i]]);
    }

    // Calculate values at barycentric coordinates
    vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    rv.uv = rv.vertices[0].uv * barycentricCoords.x + rv.vertices[1].uv * barycentricCoords.y + rv.vertices[2].uv * barycentricCoords.z;
    rv.normal = rv.vertices[0].normal * barycentricCoords.x + rv.vertices[1].normal * barycentricCoords.y + rv.vertices[2].normal * barycentricCoords.z;

    return rv;
}

void main()
{
    const Triangle triangle = unpackTriangle(gl_InstanceCustomIndexEXT, gl_PrimitiveID);

    const Material m = materials.v[triangle.material_index];
    
    const vec3 color = texture(sampler2D(textures[nonuniformEXT(m.baseColorTextureIndex)],
                        samplers[0]),
                triangle.uv).rgb;
                
    const vec3 lightVector = normalize(cam.lightPosition.xyz);
    const float dotProduct = max(dot(lightVector, triangle.normal), 0.2);
    hitValue = color * dotProduct;

    float tmin = 0.001;
    float tmax = 10000.0;
    vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    shadowed = true;  
    // Trace shadow ray and offset indices to match shadow hit/miss shader group indices
    traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 0, 0, 1, origin, tmin, lightVector, tmax, 2);
    if (shadowed) {
        hitValue *= 0.3;
    }
}
