#version 450

layout(location = 0) in vec2 inTexCoord;

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(1.0, 1.0, 1.0, texture(texSampler, inTexCoord).r);

    if (outColor.w < 0.6) {
        discard;
    }
}