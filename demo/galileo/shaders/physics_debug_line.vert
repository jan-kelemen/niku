#version 430

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform Transform {
    mat4 view;
    mat4 projection;
} transform;

layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = transform.projection * transform.view * vec4(inPosition, 1.0);

    outColor = inColor;
}

