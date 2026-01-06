#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec4 inTexColor;
layout(location = 2) in flat uint inBitmapIndex;

layout(binding = 0) uniform sampler texSampler;
layout(binding = 1) uniform texture2D texImages[];

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(inTexColor.rgb,
        texelFetch(
            sampler2D(texImages[nonuniformEXT(inBitmapIndex)], texSampler),
            ivec2(inTexCoord.x, inTexCoord.y),
            0)
            .r);
}
