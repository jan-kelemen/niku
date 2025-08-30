#ifndef VKRNDR_ACCELERATION_STRUCTURE_INCLUDED
#define VKRNDR_ACCELERATION_STRUCTURE_INCLUDED

#include <vkrndr_buffer.hpp>

#include <volk.h>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] acceleration_structure_t final
    {
        VkAccelerationStructureKHR handle{VK_NULL_HANDLE};
        vkrndr::buffer_t buffer;
        VkDeviceAddress device_address{};

        [[nodiscard]] operator VkAccelerationStructureKHR() const noexcept;
    };

    void destroy(device_t const& device,
        acceleration_structure_t const& structure);

    [[nodiscard]] VkDeviceAddress device_address(device_t const& device,
        acceleration_structure_t const& acceleration_structure);

    buffer_t create_acceleration_structure_buffer(device_t const& device,
        VkAccelerationStructureBuildSizesInfoKHR const& info);

    buffer_t create_scratch_buffer(device_t const& device,
        VkAccelerationStructureBuildSizesInfoKHR const& info);

    acceleration_structure_t create_acceleration_structure(
        device_t const& device,
        VkAccelerationStructureTypeKHR type,
        VkAccelerationStructureBuildSizesInfoKHR const& info);
} // namespace vkrndr

inline vkrndr::acceleration_structure_t::operator VkAccelerationStructureKHR()
    const noexcept
{
    return handle;
}

#endif
