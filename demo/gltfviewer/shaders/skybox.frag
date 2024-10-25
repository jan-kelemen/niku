#version 460

layout(location = 0) in vec3 inPosition;

layout(set = 1, binding = 0) uniform samplerCube environmentMap;

layout(location = 0) out vec4 outColor;

void main() { outColor = vec4(texture(environmentMap, inPosition).rgb, 1.0); }
