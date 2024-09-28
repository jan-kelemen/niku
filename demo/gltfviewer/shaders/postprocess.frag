#version 460

#extension GL_GOOGLE_include_directive : require

#include "color_conversion.glsl"

layout(location = 0) in vec2 inUV;

layout (constant_id = 0) const int SAMPLES = 8;

layout(push_constant) uniform PushConsts {
    float gamma;
    float exposure;
} pc;

layout(set = 0, binding = 0) uniform sampler2DMS backbuffer;

layout(location = 0) out vec4 outColor;

vec4 resolve(ivec2 uv) {
    vec4 result = vec4(0.0);	   
    for (int i = 0; i < SAMPLES; i++) {
        vec4 val = texelFetch(backbuffer, uv, i);
        result += val;
    }

    return result / float(SAMPLES);
}

void main() {
    // https://github.com/SaschaWillems/Vulkan/blob/master/shaders/glsl/deferredmultisampling/deferred.frag
    ivec2 attDim = textureSize(backbuffer);
    ivec2 UV = ivec2(inUV * attDim);

    vec4 result = resolve(UV);
    if (pc.exposure != 0.0) {
        result = exposureToneMapping(result, pc.exposure);
    }

    if (pc.gamma != 0.0) {
        result = gammaCorrection(result, pc.gamma);
    }

    outColor = result;
}
