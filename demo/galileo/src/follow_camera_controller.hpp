#ifndef GALILEO_FOLLOW_CAMERA_CONTROLLER_INCLUDED
#define GALILEO_FOLLOW_CAMERA_CONTROLLER_INCLUDED

namespace ngngfx
{
    class aircraft_camera_t;
} // namespace ngngfx

namespace galileo
{
    class character_t;
} // namespace galileo

namespace galileo
{
    class [[nodiscard]] follow_camera_controller_t final
    {
    public:
        explicit follow_camera_controller_t(ngngfx::aircraft_camera_t& camera);

        follow_camera_controller_t(follow_camera_controller_t const&) = delete;

        follow_camera_controller_t(
            follow_camera_controller_t&&) noexcept = delete;

    public:
        ~follow_camera_controller_t() = default;

    public:
        bool update(character_t const& character);

    public:
        follow_camera_controller_t& operator=(
            follow_camera_controller_t const&) = delete;

        follow_camera_controller_t& operator=(
            follow_camera_controller_t&&) noexcept = delete;

    private:
        ngngfx::aircraft_camera_t* camera_;
    };
} // namespace galileo

#endif
