#version 460

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConsts
{
    uint direction;
    float roughness;
} pc;

layout(set = 0, binding = 1) uniform Projection
{
    mat4 projection;
    mat4 directions[6];
} proj;

layout(location = 0) out vec3 outPosition;

void main()
{
    outPosition = inPosition;
    gl_Position =
        proj.projection * proj.directions[pc.direction] * vec4(inPosition, 1.0);
}
