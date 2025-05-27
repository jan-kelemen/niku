#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec4 inTexColor;

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(inTexColor.rgb, texelFetch(texSampler, ivec2(inTexCoord.x, inTexCoord.y), 0).r);

}