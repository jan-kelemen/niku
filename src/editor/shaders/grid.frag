#version 460

layout(location = 0) in vec2 inCoords;

layout(location = 0) out vec4 outColor;

void main() { outColor = vec4(inCoords + 0.5, 0.0, 1.0); }
