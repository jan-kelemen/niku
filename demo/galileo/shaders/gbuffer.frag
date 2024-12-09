#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outAlbedo;
layout(location = 3) out vec4 outSpecular;

void main() {
    outPosition = inPosition;
    outNormal = normalize(inNormal);
    outAlbedo = inColor;
    outSpecular = inColor;
}
