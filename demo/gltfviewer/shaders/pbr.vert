#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(push_constant) uniform PushConsts {
    uint modelIndex;
    uint materialIndex;
} pc;

layout(set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
    vec3 lightPosition;
    vec3 lightColor;
} camera;

layout(std140, set = 2, binding = 0) readonly buffer Transform {
    mat4 model[];
} transforms;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;

void main() {
    gl_Position = camera.projection * camera.view * transforms.model[pc.modelIndex] * vec4(inPosition, 1.0);

    outPosition = (transforms.model[pc.modelIndex] * vec4(inPosition, 1.0)).xyz;
    outNormal = transpose(inverse(mat3(transforms.model[pc.modelIndex]))) * inPosition;
    outUV = inUV;
}
