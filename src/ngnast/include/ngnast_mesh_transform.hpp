#ifndef NGNAST_MESH_TRANSFORM_INCLUDED
#define NGNAST_MESH_TRANSFORM_INCLUDED

namespace ngnast
{
    struct primitive_t;
} // namespace ngnast

namespace ngnast::mesh
{
    bool make_unindexed(primitive_t& primitive);

    bool make_indexed(primitive_t& primitive);
} // namespace ngnast::mesh

#endif
