#ifndef NGNPHY_COORDINATE_SYSTEM_INCLUDED
#define NGNPHY_COORDINATE_SYSTEM_INCLUDED

#include <glm/vec3.hpp>

namespace ngnphy
{
    struct [[nodiscard]] coordinate_system_t final
    {
        static constexpr glm::vec3 left{-1.0f, 0.0f, 0.0f};
        static constexpr glm::vec3 right{1.0f, 0.0f, 0.0f};
        static constexpr glm::vec3 front{0.0f, 0.0f, -1.0f};
        static constexpr glm::vec3 back{0.0f, 0.0f, 1.0f};
    };
} // namespace ngnphy

#endif
