#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "color_conversion.glsl"
#include "math_constants.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec2 inUV;

layout(push_constant) uniform PushConsts {
    uint modelIndex;
    uint materialIndex;
    uint debug;
    float ibl_factor;
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
    uint doubleSided;
};

layout(std430, set = 1, binding = 2) readonly buffer MaterialBuffer {
    Material v[];
} materials;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

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
    vec3 normal = inNormal;

    if (m.normalTextureIndex != UINT_MAX) {
        vec3 tangentNormal = texture(sampler2D(textures[nonuniformEXT(m.normalTextureIndex)], samplers[nonuniformEXT(m.normalSamplerIndex)]), inUV).rgb;
        tangentNormal = normalize(tangentNormal * 2.0 - 1.0) * vec3(m.normalScale);

        vec3 N = normalize(inNormal);
        vec3 T = normalize(inTangent.xyz);
        vec3 B = normalize(cross(N, T)) * inTangent.w;
        mat3 TBN = mat3(T, B, N);

        normal = TBN * tangentNormal;
    }

    normal = normalize(normal);
    if (m.doubleSided > 0 && !gl_FrontFacing) {
        return -normal;
    }

    return normal;
}

vec3 IBLContribution(vec3 N, vec3 reflection, float NdotV, float roughness, vec3 diffuseColor, vec3 specularColor) {
	float lod = roughness * env.prefilteredMipLevels;

	vec2 brdf = texture(samplerBRDF, vec2(NdotV, 1.0 - roughness)).rg;
	vec3 diffuseLight = texture(irradianceMap, N).rgb;

	vec3 specularLight = textureLod(prefilteredMap, reflection, lod).rgb;

	vec3 diffuse = diffuseLight * diffuseColor;
	vec3 specular = specularLight * (specularColor * brdf.x + brdf.y);

	diffuse *= pc.ibl_factor;
	specular *= pc.ibl_factor;

	return diffuse + specular;
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
    outNormal = vec4(N, 1.0);

    const vec3 V = normalize(env.cameraPosition - inPosition);
    const float NdotV = clamp(abs(dot(N, V)), 0.001, 1.0);
    const vec3 reflection = normalize(reflect(-V, N));

    const vec3 F0 = vec3(0.04);
    const vec3 diffuseColor = (1.0 - metallic) * (vec3(1.0) - F0) * albedo.rgb;
    const vec3 specularColor = mix(F0, albedo.rgb, metallic);

    outColor = vec4(IBLContribution(N, reflection, NdotV, roughness, diffuseColor, specularColor), albedo.a);
}
