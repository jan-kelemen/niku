#version 460

#extension GL_GOOGLE_include_directive : require

#include "frame_info.glsl"

layout(location = 0) in vec2 inUV;

layout(set = 1, binding = 0) uniform sampler2D positionTexture;
layout(set = 1, binding = 1) uniform sampler2D normalTexture;
layout(set = 1, binding = 2) uniform sampler2D albedoTexture;

layout(location = 0) out vec4 outColor;

void main() {
    const vec3 position = texture(positionTexture, inUV).rgb;
    const vec3 normal = texture(normalTexture, inUV).rgb;
    const vec3 albedo = texture(albedoTexture, inUV).rgb;
    
    vec3 lighting = albedo * 0.1;
    for(int i = 0; i < frame.lightCount; ++i)
    {
        Light light = lights.v[i];

        const vec3 lightDir = normalize(light.position - position);
        const vec3 diffuse = max(dot(normal, lightDir), 0.0) * albedo * light.color.rgb;
        const float distance = length(light.position - position);
        const float attenuation = 1 / (distance * distance);

        lighting += diffuse * attenuation;
    }

    outColor = vec4(lighting, 1.0);
}
