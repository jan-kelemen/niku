struct Light {
    vec4 position;
    vec4 color;
};

layout(std430, set = 0, binding = 0) readonly buffer Environment {
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
    uint lightCount;
    Light lights[];
} env;

layout(set = 0, binding = 1) uniform samplerCube irradianceMap;

