#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inUV;

layout(push_constant) uniform PushConsts {
    uint model_index;
	uint material_index;
} pc;

layout(set = 1, binding = 0) uniform texture2D textures[];
layout(set = 1, binding = 1) uniform sampler samplers[];

struct Material {
    uint texture_index;
    uint sampler_index;
};

layout(std430, set = 1, binding = 2) readonly buffer MaterialBuffer {
    Material v[];
} materials;

layout(location = 0) out vec4 outColor;

void main() {
    Material m = materials.v[pc.material_index];

    outColor = texture(nonuniformEXT(sampler2D(textures[m.texture_index], samplers[m.sampler_index])), inUV);
}
