#ifndef GLTFVIEWER_SCENE_GRAPH_INCLUDED
#define GLTFVIEWER_SCENE_GRAPH_INCLUDED

struct Transform
{
    mat4 model;
    mat4 normal;
};

layout(std140, set = 2, binding = 0) readonly buffer TransformBuffer
{
    Transform v[];
} transforms;

#endif
