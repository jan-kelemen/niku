#version 450

layout(location = 0) in vec2 inSize[];
layout(location = 1) in vec2 inTexCoords[];
layout(location = 2) in vec4 inColor[];
layout(location = 3) in uint inBitmapIndex[];

layout(vertices = 1) out;

layout(location = 0) out vec2 outSize[];
layout(location = 1) out vec2 outTexCoords[];
layout(location = 2) out vec4 outColor[];
layout(location = 3) out uint outBitmapIndex[];

void main(void)
{
    if (gl_InvocationID == 0)
    {
        gl_TessLevelOuter[0] = 1.0;
        gl_TessLevelOuter[1] = 1.0;
        gl_TessLevelOuter[2] = 1.0;
        gl_TessLevelOuter[3] = 1.0;

        gl_TessLevelInner[0] = 1.0;
        gl_TessLevelInner[1] = 1.0;
    }

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    outSize[gl_InvocationID] = inSize[gl_InvocationID];
    outTexCoords[gl_InvocationID] = inTexCoords[gl_InvocationID];
    outColor[gl_InvocationID] = inColor[gl_InvocationID];
    outBitmapIndex[gl_InvocationID] = inBitmapIndex[gl_InvocationID];
}
