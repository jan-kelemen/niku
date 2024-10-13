#version 460 

#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 inPosition;

#include "environment.glsl" // (set = 0)

layout(location = 0) out vec3 outPosition; 

void main() {
    gl_Position = env.projection * env.view * vec4(inPosition, 1.0);

    outPosition = inPosition;  
}
