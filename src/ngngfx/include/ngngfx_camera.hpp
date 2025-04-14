#ifndef NGNGFX_CAMERA_INCLUDED
#define NGNGFX_CAMERA_INCLUDED

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace ngngfx
{
    class [[nodiscard]] camera_t
    {
    public:
        camera_t();

        camera_t(glm::vec3 const& position);

        camera_t(camera_t const&) = default;

        camera_t(camera_t&&) = default;

    public:
        virtual ~camera_t() = default;

    public:
        virtual void update() = 0;

        [[nodiscard]] virtual glm::mat4 const& view_matrix() const = 0;

    public:
        virtual void set_position(glm::vec3 const& position);

        [[nodiscard]] virtual glm::vec3 const& position() const;

    public:
        camera_t& operator=(camera_t const&) = default;

        camera_t& operator=(camera_t&&) noexcept = default;

    protected:
        glm::vec3 position_;
    };
} // namespace ngngfx
#endif
