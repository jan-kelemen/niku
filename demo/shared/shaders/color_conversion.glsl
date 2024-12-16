#ifndef NIKU_DEMO_COLOR_CONVERSION_INCLUDED
#define NIKU_DEMO_COLOR_CONVERSION_INCLUDED

#extension GL_GOOGLE_include_directive : require

#include "pbrNeutral.glsl"

// https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
vec3 fromLinear(vec3 linearRGB)
{
    bvec3 cutoff = lessThan(linearRGB, vec3(0.0031308));
    vec3 higher =
        vec3(1.055) * pow(linearRGB, vec3(1.0 / 2.4)) - vec3(0.055);
    vec3 lower = linearRGB * vec3(12.92);

    return mix(higher, lower, cutoff);
}

vec4 fromLinear(vec4 linearRGB) 
{
    return vec4(fromLinear(linearRGB.rgb), linearRGB.a);
}

vec3 toLinear(vec3 sRGB)
{
    bvec3 cutoff = lessThan(sRGB, vec3(0.04045));
    vec3 higher = pow((sRGB + vec3(0.055)) / vec3(1.055), vec3(2.4));
    vec3 lower = sRGB / vec3(12.92);

    return mix(higher, lower, cutoff);
}

vec4 toLinear(vec4 linearRGB) 
{
    return vec4(toLinear(linearRGB.rgb), linearRGB.a);
}

vec3 pbrNeutralToneMapping(vec3 linearRGB)
{
    return PBRNeutralToneMapping(linearRGB.rgb);
}

vec4 pbrNeutralToneMapping(vec4 linearRGB)
{
    return vec4(PBRNeutralToneMapping(linearRGB.rgb), linearRGB.a);
}

vec4 exposureToneMapping(vec4 linearRGB, float exposure)
{
    return vec4(vec3(1.0) - exp(-linearRGB.rgb * exposure), linearRGB.a);
}

vec4 gammaCorrection(vec4 linearRGB, float gamma)
{
    return vec4(pow(linearRGB.rgb, vec3(1.0 / gamma)), linearRGB.a);
}

float luminance(vec3 linearRGB)
{
    return dot(linearRGB, vec3(0.2126, 0.7152, 0.0722));
}

float luminance(vec4 linearRGB) { return luminance(linearRGB.rgb); }

float luma(vec3 linearRGB)
{
    return dot(linearRGB, vec3(0.299, 0.587, 0.114));
}

float luma(vec4 linearRGB) { return luma(linearRGB.rgb); }

#endif
