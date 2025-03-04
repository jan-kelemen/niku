#ifndef NGNPHY_JOLT_GEOMETRY_INCLUDED
#define NGNPHY_JOLT_GEOMETRY_INCLUDED

#include <ngnphy_jolt.hpp>

#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Math/Float3.h>

#include <algorithm>
#include <ranges>
#include <type_traits>

namespace ngnphy
{
    template<std::ranges::random_access_range Triangles, typename Op>
    requires(std::is_convertible_v<std::invoke_result_t<Op,
                                       std::ranges::range_value_t<Triangles>,
                                       std::ranges::range_value_t<Triangles>,
                                       std::ranges::range_value_t<Triangles>>,
        JPH::Triangle>)
    [[nodiscard]] JPH::TriangleList
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
    to_unindexed_triangles(Triangles&& triangles, Op func)
    {
        JPH::TriangleList rv;

        rv.reserve(std::ranges::size(triangles) / 3);

        for (auto it{std::ranges::begin(triangles)};
            it != std::ranges::end(triangles);
            std::advance(it, 3))
        {
            rv.push_back(func(*it, *(it + 1), *(it + 2)));
        }

        return rv;
    }

    template<std::ranges::random_access_range Indices, typename Op>
    requires(std::is_convertible_v<std::invoke_result_t<Op,
                                       std::ranges::range_value_t<Indices>,
                                       std::ranges::range_value_t<Indices>,
                                       std::ranges::range_value_t<Indices>>,
        JPH::IndexedTriangle>)
    [[nodiscard]] JPH::IndexedTriangleList
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
    to_indexed_triangles(Indices&& indices, Op func)
    {
        JPH::IndexedTriangleList rv;

        rv.reserve(std::ranges::size(indices) / 3);

        for (auto it{std::ranges::begin(indices)};
            it != std::ranges::end(indices);
            std::advance(it, 3))
        {
            rv.push_back(func(*it, *(it + 1), *(it + 2)));
        }

        return rv;
    }

    template<std::ranges::range Vertices, typename Op>
    requires(std::is_convertible_v<
        std::invoke_result_t<Op, std::ranges::range_value_t<Vertices>>,
        JPH::Float3>)
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
    [[nodiscard]] JPH::VertexList to_vertices(Vertices&& vertices, Op func)
    {
        JPH::VertexList rv;

        if constexpr (std::ranges::sized_range<Vertices>)
        {
            rv.reserve(std::ranges::size(vertices));
        }

        std::ranges::transform(vertices, std::back_inserter(rv), func);

        return rv;
    }
} // namespace ngnphy

#endif
