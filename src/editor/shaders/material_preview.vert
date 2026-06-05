#version 460

layout(std430, set = 0, binding = 0) restrict readonly buffer CameraInfo
{
    mat4 view;
    mat4 projection;
    vec3 position;
} camera;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec2 outUV;

void main()
{
    gl_Position = camera.projection * camera.view * vec4(inPosition, 1);

    outPosition = inPosition;
    outUV = inUV;
}
