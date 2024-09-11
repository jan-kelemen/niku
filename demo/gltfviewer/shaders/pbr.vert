#version 460

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
} camera;

layout(location = 0) out vec3 outColor;

void main() {
    gl_Position = camera.projection * camera.view * vec4(inPosition / 2, 1.0);
    outColor = inPosition;
}
