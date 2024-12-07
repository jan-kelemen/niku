#version 460

layout(location = 0) in vec4 inColor;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;
layout(location = 3) out vec4 outSpecular;

void main() {
    outPosition = inColor;
    outNormal = inColor;
    outAlbedo = inColor;
    outSpecular = inColor;
}
