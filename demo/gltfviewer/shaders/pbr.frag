#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "color_conversion.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUV;

layout(push_constant) uniform PushConsts {
    uint modelIndex;
    uint materialIndex;
    uint debug;
} pc;

#include "environment.glsl" // (set = 0)

layout(set = 1, binding = 0) uniform texture2D textures[];
layout(set = 1, binding = 1) uniform sampler samplers[];

struct Material {
    vec4 baseColorFactor;
    vec3 emissiveFactor;
    uint baseColorTextureIndex;
    uint baseColorSamplerIndex;
    float alphaCutoff;
    uint metallicRoughnessTextureIndex;
    uint metallicRoughnessSamplerIndex;
    float metallicFactor;
    float roughnessFactor;
    float occlusionStrength;
    uint normalTextureIndex;
    uint normalSamplerIndex;
    uint emissiveTextureIndex;
    uint emissiveSamplerIndex;
    uint occlusionTextureIndex;
    uint occlusionSamplerIndex;
    float normalScale;
};

layout(std430, set = 1, binding = 2) readonly buffer MaterialBuffer {
    Material v[];
} materials;

layout(location = 0) out vec4 outColor;

const uint UINT_MAX = ~0;
const float M_PI = 3.141592653589793;

vec4 baseColor(Material m) {
    vec4 color = vec4(1);
    if (m.baseColorTextureIndex != UINT_MAX) {
        color = texture(sampler2D(textures[nonuniformEXT(m.baseColorTextureIndex)], samplers[nonuniformEXT(m.baseColorSamplerIndex)]), inUV);
    }

    return m.baseColorFactor * color;
}

void metallicRoughness(Material m, out float metallic, out float roughness) {
    roughness = m.roughnessFactor;
    metallic = m.metallicFactor;

    if (m.metallicRoughnessTextureIndex != UINT_MAX) {
        vec4 mr = texture(sampler2D(textures[nonuniformEXT(m.metallicRoughnessTextureIndex)], samplers[nonuniformEXT(m.metallicRoughnessSamplerIndex)]), inUV);
        roughness *= mr.g;
        metallic *= mr.b;
    }
    else {
        roughness = clamp(roughness, 0.04, 1.0);
        metallic = clamp(metallic, 0.0, 1.0);
    }

}

vec3 worldNormal(Material m) {
    if (m.normalTextureIndex != UINT_MAX) {
        vec3 tangentNormal = texture(sampler2D(textures[nonuniformEXT(m.normalTextureIndex)], samplers[nonuniformEXT(m.normalSamplerIndex)]), inUV).rgb;
        tangentNormal = normalize(tangentNormal * 2.0 - 1.0) * vec3(m.normalScale);

        vec3 Q1 = dFdx(inPosition);
        vec3 Q2 = dFdy(inPosition);
        vec2 st1 = dFdx(inUV);
        vec2 st2 = dFdy(inUV);

        vec3 N = normalize(inNormal);
        vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
        vec3 B = -normalize(cross(N, T));
        mat3 TBN = mat3(T, B, N);

        return normalize(TBN * tangentNormal);
    }
    else {
        return normalize(inNormal);
    }
}

float ambientOcclusion(Material m) {
    float occlusion = 1.0;
    if (m.occlusionTextureIndex != UINT_MAX) {
        occlusion = texture(sampler2D(textures[nonuniformEXT(m.occlusionTextureIndex)], samplers[nonuniformEXT(m.occlusionSamplerIndex)]), inUV).r;
    }

    return occlusion;
}

vec3 emissiveColor(Material m) {
    vec3 emissive = m.emissiveFactor.rgb;
    if (m.emissiveTextureIndex != UINT_MAX) {
        emissive *= texture(sampler2D(textures[nonuniformEXT(m.emissiveTextureIndex)], samplers[nonuniformEXT(m.emissiveSamplerIndex)]), inUV).rgb;
    };

    return emissive;
}

vec3 fresnelSchlick(vec3 f0, vec3 f90, float VdotH) {
    return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

float geometricOcclusion(float roughness, float NdotL, float NdotV) {
    const float r = roughness * roughness;
    const float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r  + (1.0 - r) * (NdotL * NdotL)));
    const float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r + (1.0 - r) * (NdotV * NdotV)));

    return attenuationL * attenuationV;
}

float microfacetDistribution(float roughness, float NdotH) {
    const float r = roughness * roughness;
    const float f = (NdotH * r - NdotH) * NdotH + 1.0;
    return r / (M_PI * f * f);
}

vec3 diffuse(vec3 diffuseColor) {
    return diffuseColor / M_PI;
}

void main() {
    Material m = materials.v[pc.materialIndex];

    vec4 albedo = baseColor(m);
    if (m.alphaCutoff.x != 0.0 && albedo.a < m.alphaCutoff.x) {
        discard;
    }
    albedo *= inColor;

    float metallic;
    float roughness;
    metallicRoughness(m, metallic, roughness);
    const float alphaRoughness = roughness * roughness;

    const vec3 N = worldNormal(m);
    const vec3 V = normalize(env.cameraPosition - inPosition);
    const vec3 L = normalize(env.lightPosition);
    const vec3 H = normalize(L + V);

    const vec3 F0 = vec3(0.04);
    const vec3 diffuseColor = (1.0 - metallic) * (vec3(1.0) - F0) * albedo.rgb;

    const vec3 specularColor = mix(F0, albedo.rgb, metallic);
    const float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
    const float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    const vec3 specularEnvironmentR0 = specularColor.rgb;
    const vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    const float VdotH = clamp(dot(V, H), 0.0, 1.0);
    const float NdotL = clamp(dot(N, L), 0.001, 1.0);
    const float NdotV = clamp(abs(dot(N, V)), 0.001, 1.0);
    const float NdotH = clamp(dot(N, H), 0.0, 1.0);

    const vec3 F = fresnelSchlick(specularEnvironmentR0, specularEnvironmentR90, VdotH);
    const float G = geometricOcclusion(alphaRoughness, NdotL, NdotV);
    const float D = microfacetDistribution(alphaRoughness, NdotH);

    const vec3 diffuseContribution = (1.0 - F) * diffuse(diffuseColor);
    const vec3 specularContribution = F * G * D / (4.0 * NdotL * NdotV);

    vec3 color = NdotL * env.lightColor * (diffuseContribution + specularContribution);

    const float occlusion = ambientOcclusion(m);
    color = mix(color, color * occlusion, m.occlusionStrength);

    const vec3 emissive = emissiveColor(m);
    color += emissive;

    outColor = vec4(color, pc.debug > 0 ? 1.0 : albedo.a);

    if (pc.debug > 0) {
        switch (pc.debug) {
            case 1:
                outColor.rgba = albedo;
                break;
            case 2:
                outColor.rgb = N;
                break;
            case 3:
                outColor.rgb = vec3(occlusion);
                break;
            case 4:
                outColor.rgb = emissive;
                break;
            case 5:
                outColor.rgb = vec3(metallic);
                break;
            case 6:
                outColor.rgb = vec3(roughness);
                break;
            case 7:
                outColor.rgb = diffuseContribution;
                break;
            case 8:
                outColor.rgb = F;
                break;
            case 9:
                outColor.rgb = vec3(G);
                break;
            case 10:
                outColor.rgb = vec3(D);
                break;
            case 11:
                outColor.rgb = specularContribution;
                break;
        }
    }
}
