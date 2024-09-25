#version 460

layout(location = 0) in vec2 inUV;

layout(push_constant) uniform PushConsts {
    uint samples;
    float gamma;
    float exposure;
} pc;

layout(set = 0, binding = 0) uniform sampler2DMS backbuffer;

layout(location = 0) out vec4 outColor;

vec4 resolve(ivec2 uv) {
    vec4 result = vec4(0.0);	   
    for (int i = 0; i < pc.samples; i++) {
        vec4 val = texelFetch(backbuffer, uv, i);
        result += val;
    }

    return result / float(pc.samples);
}

void main() {
    // https://github.com/SaschaWillems/Vulkan/blob/master/shaders/glsl/deferredmultisampling/deferred.frag
    ivec2 attDim = textureSize(backbuffer);
    ivec2 UV = ivec2(inUV * attDim);

    vec4 resolved = resolve(UV);
  
    vec3 color = resolved.rgb;

    // exposure tone mapping
    if (pc.exposure != 0.0) {
        color = vec3(1.0) - exp(-color * pc.exposure);
    }

    // gamma correction 
    if (pc.gamma != 0.0) {
        color = pow(color, vec3(1.0 / pc.gamma));
    }

    outColor = vec4(color, resolved.a);
}
