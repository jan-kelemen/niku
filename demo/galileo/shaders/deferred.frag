#version 460

#extension GL_GOOGLE_include_directive : require

#include "frame_info.glsl"

layout(location = 0) in vec2 inUV;

layout(set = 1, binding = 0) uniform sampler2D positionTexture;
layout(set = 1, binding = 1) uniform sampler2D normalTexture;
layout(set = 1, binding = 2) uniform sampler2D albedoTexture;
layout(set = 1, binding = 3) uniform sampler2D specularTexture;

layout(location = 0) out vec4 outColor;

void main() {
    const vec3 position = texture(positionTexture, inUV).rgb;
    const vec3 normal = texture(normalTexture, inUV).rgb;
    const vec3 albedo = texture(albedoTexture, inUV).rgb;
    
    vec3 lighting = albedo * 0.1;
    vec3 lightDir = normalize(vec3(0, 5, 0) - position);
    vec3 diffuse = max(dot(normal, lightDir), 0.0) * albedo;
    lighting += diffuse;

    outColor = vec4(lighting, 1.0);
}
