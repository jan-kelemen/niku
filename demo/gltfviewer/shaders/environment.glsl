layout(set = 0, binding = 0) uniform Environment {
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
    vec3 lightPosition;
    vec3 lightColor;
} env;
