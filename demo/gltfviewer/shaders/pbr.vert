#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(push_constant) uniform PushConsts {
    uint model_index;
	uint material_index;
} pc;

layout(set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
} camera;

layout(std140, set = 2, binding = 0) readonly buffer Transform {
    mat4 model[];
} transforms;

layout(location = 0) out vec2 outUV;

void main() {
    gl_Position = camera.projection * camera.view * transforms.model[pc.model_index] * vec4(inPosition, 1.0);

    outUV = inUV;
}
