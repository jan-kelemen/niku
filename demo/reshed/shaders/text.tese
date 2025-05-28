#version 450

layout (quads, equal_spacing, ccw) in;

layout (location = 0) in vec2 inSize[];
layout (location = 1) in vec2 inTexCoords[];
layout (location = 2) in vec4 inColor[];

layout (push_constant) uniform PushConst{
    mat4 projection;
};

layout (location = 0) out vec2 outTexCoords;
layout (location = 1) out vec4 outTexColor;

void main(void)
{
    vec2 newPosition = gl_in[0].gl_Position.xy;
    newPosition.x += inSize[0].x - gl_TessCoord.x * inSize[0].x;
    newPosition.y -= gl_TessCoord.y * inSize[0].y;

    gl_Position = projection * vec4(newPosition, 0.0, 1.0);

    outTexCoords = inTexCoords[0] + (inSize[0] - gl_TessCoord.xy * inSize[0]);
    outTexColor = inColor[0];
}