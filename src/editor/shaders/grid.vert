#version 460

layout(std430, set = 0, binding = 0) restrict readonly buffer CameraInfo
{
    mat4 view;
    mat4 projection;
    vec3 position;
} camera;

layout(location = 0) in vec4 inPosition;

layout(location = 0) out vec2 outCoords;

void main()
{
    vec4 worldPosition = inPosition;
    worldPosition.xyz *= 1000;
    worldPosition.xz += camera.position.xz;

    gl_Position = camera.projection * camera.view * worldPosition;

    outCoords = worldPosition.xz;
}
