#version 460

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outColor;

void main() {
    gl_Position = vec4(inPosition / 2, 1.0);
    outColor = vec3(0);
}
