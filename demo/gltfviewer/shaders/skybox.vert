#version 460

#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 inPosition;

#include "environment.glsl" // (set = 0)

layout(location = 0) out vec3 outPosition;

void main()
{
    mat4 view = mat4(mat3(env.view));
    vec4 clipPosition = env.projection * view * vec4(inPosition, 1.0);

    gl_Position = clipPosition.xyww;

    outPosition = inPosition;
}
