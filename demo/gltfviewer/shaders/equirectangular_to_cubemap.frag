#version 460

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform sampler2D equirectangularMap;

layout(location = 0) out vec4 outColor;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 sampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    const vec2 uv = sampleSphericalMap(normalize(inPosition));
    const vec3 color = texture(equirectangularMap, uv).rgb;
    outColor = vec4(color, 1.0);
}
