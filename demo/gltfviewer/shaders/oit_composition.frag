#version 460 core

layout(location = 0) in vec2 inUV;

layout(constant_id = 0) const int SAMPLES = 8;

layout(set = 0, binding = 0) uniform sampler2DMS accumulation;
layout(set = 0, binding = 1) uniform sampler2DMS reveal;

layout(location = 0) out vec4 outColor;

const float EPSILON = 0.00001f;

vec4 resolveAccumulation(ivec2 uv)
{
    vec4 result = vec4(0.0);
    for (int i = 0; i < SAMPLES; i++)
    {
        vec4 val = texelFetch(accumulation, uv, i);
        result += val;
    }

    return result / float(SAMPLES);
}

float resolveReveal(ivec2 uv)
{
    float result = 0.0;
    for (int i = 0; i < SAMPLES; i++)
    {
        const float val = texelFetch(reveal, uv, i).r;
        result += val;
    }

    return result / float(SAMPLES);
}

bool isApproximatelyEqual(float a, float b)
{
	return abs(a - b) <= (abs(a) < abs(b) ? abs(b) : abs(a)) * EPSILON;
}

float max3(vec3 v) 
{
	return max(max(v.x, v.y), v.z);
}

void main()
{
    ivec2 attDim = textureSize(accumulation);
    ivec2 UV = ivec2(inUV * attDim);

    const float revealage = resolveReveal(UV);
	
	if (isApproximatelyEqual(revealage, 1.0f)) 
		discard;
 
	vec4 accumulation = resolveAccumulation(UV);
	
	if (isinf(max3(abs(accumulation.rgb)))) 
		accumulation.rgb = vec3(accumulation.a);

	vec3 average_color = accumulation.rgb / max(accumulation.a, EPSILON);

	outColor = vec4(average_color, revealage);
}
