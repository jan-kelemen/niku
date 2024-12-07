#ifndef GALILEO_RENDER_GRAPH_INCLUDED
#define GALILEO_RENDER_GRAPH_INCLUDED

layout(std430, set = 1, binding = 0) readonly buffer RenderGraph
{
    mat4 position[];
} graph;

#endif
