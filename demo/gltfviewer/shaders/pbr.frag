#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 inFragmentPosition;
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

layout(set = 1, binding = 0) uniform texture2D textures[];
layout(set = 1, binding = 1) uniform sampler samplers[];

struct Material {
    vec4 baseColorFactor;
    uint baseColorTextureIndex;
    uint baseColorSamplerIndex;
};

layout(std430, set = 1, binding = 2) readonly buffer MaterialBuffer {
    Material v[];
} materials;

layout(location = 0) out vec4 outColor;

const uint UINT_MAX = ~0;

vec4 baseColor() {
    Material m = materials.v[pc.materialIndex];

    vec4 color = vec4(1);
    if (m.baseColorTextureIndex != UINT_MAX) {
        color = texture(nonuniformEXT(sampler2D(textures[m.baseColorTextureIndex], samplers[m.baseColorSamplerIndex])), inUV);
    }

    return m.baseColorFactor * color;
}

void main() {
    vec3 cameraPosition = camera.cameraPosition;
    vec3 lightColor = camera.lightColor;
    vec3 lightPosition = camera.lightPosition;

    vec3 norm = normalize(inNormal);
    vec3 lightDirection = normalize(lightPosition - inFragmentPosition);  

    float diff = max(dot(norm, lightDirection), 0.1);
    vec3 diffuse = diff * lightColor;

    float specularStrength = 0.05;
    vec3 viewDir = normalize(cameraPosition - inFragmentPosition);
    vec3 reflectDirection = reflect(-lightDirection, norm);  
    float spec = pow(max(dot(viewDir, reflectDirection), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;  

    vec4 result = vec4(diffuse + specular, 1.0) * baseColor();

    outColor = result;
}
