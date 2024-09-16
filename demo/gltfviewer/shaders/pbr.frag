#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inUV;

layout(push_constant) uniform PushConsts {
    uint modelIndex;
    uint materialIndex;
} pc;

layout(set = 1, binding = 0) uniform texture2D textures[];
layout(set = 1, binding = 1) uniform sampler samplers[];

struct Material {
    vec4 baseColorFactor;
    uint baseColorTextureIndex;
    uint baseColorSamplerIndex;
};

layout(std430, set = 1, binding = 2) readonly buffer MaterialBuffer {
    Material v[];
} materials;

layout(location = 0) out vec4 outColor;

const uint UINT_MAX = ~0;

vec4 baseColor() {
    Material m = materials.v[pc.materialIndex];

    vec4 color = vec4(1);
    if (m.baseColorTextureIndex != UINT_MAX) {
        color = texture(nonuniformEXT(sampler2D(textures[m.baseColorTextureIndex], samplers[m.baseColorSamplerIndex])), inUV);
    }

    return m.baseColorFactor * color;
}

void main() {
    outColor = baseColor();
}
