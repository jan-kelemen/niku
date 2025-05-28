#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inSize;
layout(location = 2) in vec2 inTexCoords;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec2 outSize;
layout(location = 1) out vec2 outTexCoords;
layout(location = 2) out vec4 outColor;

void main()
{
    gl_Position = vec4(inPos, 0.0, 1.0);
    outSize = inSize;
    outTexCoords = inTexCoords;
    outColor = inColor;
}
