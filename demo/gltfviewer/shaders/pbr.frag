#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 inFragmentPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in mat3 inTBN;

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
    float alphaCutoff;
    uint metallicRoughnessTextureIndex;
    uint metallicRoughnessSamplerIndex;
    float metallicFactor;
    float roughnessFactor;
    uint normalTextureIndex;
    uint normalSamplerIndex;
    float normalScale;
};

layout(std430, set = 1, binding = 2) readonly buffer MaterialBuffer {
    Material v[];
} materials;

layout(location = 0) out vec4 outColor;

const uint UINT_MAX = ~0;

vec4 baseColor(Material m) {
    vec4 color = vec4(1);
    if (m.baseColorTextureIndex != UINT_MAX) {
        color = texture(nonuniformEXT(sampler2D(textures[m.baseColorTextureIndex], samplers[m.baseColorSamplerIndex])), inUV);
    }

    return m.baseColorFactor * color;
}

vec3 tangentNormal(Material m)
{
    if (m.normalTextureIndex != UINT_MAX) {
        vec3 normal = texture(nonuniformEXT(sampler2D(textures[m.normalTextureIndex], samplers[m.normalSamplerIndex])), inUV).rgb;
        return normalize(normal * 2.0 - 1.0) * vec3(m.normalScale);
    }
    else {
        return normalize(inTBN * inNormal);
    }
}

void metallicRoughness(Material m, out float metallic, out float roughness) {
    roughness = m.roughnessFactor;
    metallic = m.metallicFactor;

    if (m.metallicRoughnessTextureIndex != UINT_MAX) {
        vec4 mr = texture(nonuniformEXT(sampler2D(textures[m.metallicRoughnessTextureIndex], samplers[m.metallicRoughnessSamplerIndex])), inUV);
        roughness *= mr.g;
        metallic *= mr.b;
    }
    else {
        roughness = clamp(roughness, 0.04, 1.0);
        metallic = clamp(metallic, 0.0, 1.0);
    }

}

void main() {
    vec3 cameraPosition = camera.cameraPosition;
    vec3 lightColor = camera.lightColor;
    vec3 lightPosition = camera.lightPosition;

    Material m = materials.v[pc.materialIndex];

    vec4 albedo = baseColor(m);
    if (albedo.w < m.alphaCutoff.x) {
        discard;
    }

    vec3 lightDirection = inTBN * normalize(lightPosition - inFragmentPosition);  
    vec3 normal = tangentNormal(m);

    float diff = max(dot(lightDirection, normal), 0.1);

    vec3 viewDirection = inTBN * normalize(cameraPosition - inFragmentPosition);    
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float specularAngle = max(dot(halfDirection, normal), 0.0);
    float specular = pow(specularAngle, 16);

    outColor = vec4((diff + specular) * lightColor, 1.0) * baseColor(m);
}
