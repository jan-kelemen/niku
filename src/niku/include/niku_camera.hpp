#ifndef NIKU_CAMERA_INCLUDED
#define NIKU_CAMERA_INCLUDED

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace niku
{
    class [[nodiscard]] camera
    {
    public:
        camera();

        camera(glm::vec3 const& position, float aspect_ratio);

        camera(camera const&) = default;

        camera(camera&&) = default;

    public:
        virtual ~camera() = default;

    public:
        virtual void update() = 0;

        [[nodiscard]] virtual glm::mat4 const& view_matrix() const = 0;

        [[nodiscard]] virtual glm::mat4 const& projection_matrix() const = 0;

        [[nodiscard]] virtual glm::mat4 const&
        view_projection_matrix() const = 0;

    public:
        virtual void set_aspect_ratio(float aspect_ratio);

        [[nodiscard]] virtual float aspect_ratio() const;

        virtual void set_position(glm::vec3 const& position);

        [[nodiscard]] virtual glm::vec3 const& position() const;

    public:
        camera& operator=(camera const&) = default;

        camera& operator=(camera&&) noexcept = default;

    protected:
        glm::vec3 position_;
        float aspect_ratio_;
    };
} // namespace niku
#endif
