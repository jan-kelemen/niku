#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "frame_info.glsl"
#include "materials.glsl"
#include "math_constants.glsl"
#include "render_graph.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) flat in uint inInstance;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outAlbedo;

vec4 baseColor(Material m)
{
    vec4 color = vec4(1);
    if (m.baseColorTextureIndex != UINT_MAX)
    {
        color =
            texture(sampler2D(textures[nonuniformEXT(m.baseColorTextureIndex)],
                        samplers[nonuniformEXT(m.baseColorSamplerIndex)]),
                inUV);
    }

    return m.baseColorFactor * color;
}

void main() {
    const uint material_idx = graph.nodes[inInstance].material;
    const Material m = materials.v[material_idx];

    vec4 albedo = baseColor(m);
    if (m.alphaCutoff.x != 0.0 && albedo.a < m.alphaCutoff.x)
    {
        discard;
    }
    albedo *= inColor;

    outPosition = inPosition;
    outNormal = normalize(inNormal);
    outAlbedo = albedo;
}
