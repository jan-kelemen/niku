#version 460 

#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConsts {
    uint direction;
} pc;

#include "environment.glsl" // (set = 0)

layout(set = 1, binding = 1) uniform Projection {
    mat4 projection;
    mat4 directions[6];
} proj;

layout(location = 0) out vec3 outPosition; 

void main() {

    outPosition = inPosition;
    gl_Position =  proj.projection * proj.directions[pc.direction] * vec4(inPosition, 1.0);
}