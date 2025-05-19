#version 450

layout (location=0) in vec2 inPos;
layout (location=1) in vec2 inTexCoords;

layout (location=0) out vec2 outTexCoords;

layout (push_constant) uniform PushConst{
    mat4 projection;
};

void main() {
    gl_Position = projection * vec4(inPos, 0.0, 1.0);
    outTexCoords = inTexCoords;
}
