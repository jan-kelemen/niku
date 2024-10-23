#extension GL_GOOGLE_include_directive : require

#include "pbrNeutral.glsl"

// https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
vec4 fromLinear(vec4 linearRGB) {
    bvec3 cutoff = lessThan(linearRGB.rgb, vec3(0.0031308));
    vec3 higher = vec3(1.055) * pow(linearRGB.rgb, vec3(1.0/2.4)) - vec3(0.055);
    vec3 lower = linearRGB.rgb * vec3(12.92);

    return vec4(mix(higher, lower, cutoff), linearRGB.a);
}

vec4 toLinear(vec4 sRGB) {
    bvec3 cutoff = lessThan(sRGB.rgb, vec3(0.04045));
    vec3 higher = pow((sRGB.rgb + vec3(0.055)) / vec3(1.055), vec3(2.4));
    vec3 lower = sRGB.rgb/vec3(12.92);

    return vec4(mix(higher, lower, cutoff), sRGB.a);
}

vec4 pbrNeutralToneMapping(vec4 linearRGB) {
    return vec4(PBRNeutralToneMapping(linearRGB.rgb), linearRGB.a);
}

vec4 exposureToneMapping(vec4 linearRGB, float exposure) {
    return vec4(vec3(1.0) - exp(-linearRGB.rgb * exposure), linearRGB.a);
}

vec4 gammaCorrection(vec4 linearRGB, float gamma) {
    return vec4(pow(linearRGB.rgb, vec3(1.0 / gamma)), linearRGB.a);
}
