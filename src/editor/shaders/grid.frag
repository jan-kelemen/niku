#version 460

layout(std430, set = 0, binding = 0) restrict readonly buffer CameraInfo
{
    mat4 view;
    mat4 projection;
    vec3 position;
} camera;

layout(location = 0) in vec2 inCoords;

layout(location = 0) out vec4 outColor;

// https://bgolus.medium.com/the-best-darn-grid-shader-yet-727f9278b9d8
float PristineGrid(const vec2 uv, vec2 lineWidth)
{
    lineWidth = clamp(lineWidth, 0.0, 1.0);

    const vec4 uvDDXY = vec4(dFdx(uv), dFdy(uv));
    const vec2 uvDeriv = vec2(length(uvDDXY.xz), length(uvDDXY.yw));

    const bvec2 invertLine = greaterThan(lineWidth, vec2(0.5));
    const vec2 targetWidth = mix(lineWidth, 1.0 - lineWidth, invertLine);

    const vec2 drawWidth = clamp(targetWidth, uvDeriv, vec2(0.5));
    const vec2 lineAA = max(uvDeriv, vec2(0.000001)) * 1.5;

    vec2 gridUV = abs(fract(uv) * 2.0 - 1.0);
    gridUV = mix(1.0 - gridUV, gridUV, invertLine);

    vec2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= clamp(targetWidth / drawWidth, 0.0, 1.0);

    grid2 = mix(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0.0, 1.0));
    grid2 = mix(grid2, 1.0 - grid2, invertLine);

    return mix(grid2.x, 1.0, grid2.y);
}

void main()
{
    const vec4 baseColor = vec4(vec3(0.2), 1.0);
    const vec4 lineColor = vec4(vec3(1), 1.0);

    outColor = mix(baseColor, lineColor, lineColor.a * PristineGrid(inCoords, vec2(0.1)));

    // https://dev.to/javiersalcedopuyo/simple-infinite-grid-shader-5fah
    const float fragmentDistance = length(inCoords - camera.position.xz);
    const float fadeDistance = clamp(abs(camera.position.y) * 25, 450, 500);
    outColor.a *= smoothstep(1.0, 0.0, fragmentDistance / fadeDistance);
}
