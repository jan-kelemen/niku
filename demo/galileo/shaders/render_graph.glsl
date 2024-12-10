#ifndef GALILEO_RENDER_GRAPH_INCLUDED
#define GALILEO_RENDER_GRAPH_INCLUDED

struct GraphNode
{
    mat4 model;
    uint material;
};

layout(std430, set = 2, binding = 0) readonly buffer GraphNodeBuffer
{
    GraphNode nodes[];
} graph;

#endif
