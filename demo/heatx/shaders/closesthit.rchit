#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

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

#include "materials.glsl" // set = 1

#include "geometry.glsl"

layout(push_constant) uniform PushConsts { PrimitiveBuffer primitives; } pc;

void main()
{
    const Triangle triangle = unpackTriangle(pc.primitives,
        gl_InstanceCustomIndexEXT,
        gl_PrimitiveID,
        attribs);

    const Material m = materials.v[triangle.material_index];

    const vec3 color =
        texture(sampler2D(textures[nonuniformEXT(m.baseColorTextureIndex)],
                    samplers[0]),
            triangle.uv)
            .rgb;

    const vec3 lightVector = normalize(cam.lightPosition.xyz);
    const float dotProduct = max(dot(lightVector, triangle.normal), 0.2);
    hitValue = color * dotProduct;

    float tmin = 0.001;
    float tmax = 10000.0;
    vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    shadowed = true;
    // Trace shadow ray and offset indices to match shadow hit/miss shader group
    // indices
    traceRayEXT(topLevelAS,
        gl_RayFlagsTerminateOnFirstHitEXT |
            gl_RayFlagsOpaqueEXT |
            gl_RayFlagsSkipClosestHitShaderEXT,
        0xFF,
        0,
        0,
        1,
        origin,
        tmin,
        lightVector,
        tmax,
        2);
    if (shadowed)
    {
        hitValue *= 0.3;
    }
}
