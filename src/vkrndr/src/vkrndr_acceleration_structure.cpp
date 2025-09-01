#include <vkrndr_acceleration_structure.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

void vkrndr::destroy(device_t const& device,
    acceleration_structure_t const& structure)
{
    vkDestroyAccelerationStructureKHR(device, structure.handle, nullptr);
    destroy(device, structure.buffer);
}

VkDeviceAddress vkrndr::device_address(device_t const& device,
    acceleration_structure_t const& acceleration_structure)
{
    VkAccelerationStructureDeviceAddressInfoKHR const da{
        .sType = vku::GetSType<VkAccelerationStructureDeviceAddressInfoKHR>(),
        .accelerationStructure = acceleration_structure,
    };
    return vkGetAccelerationStructureDeviceAddressKHR(device, &da);
}

vkrndr::buffer_t vkrndr::create_acceleration_structure_buffer(
    device_t const& device,
    VkAccelerationStructureBuildSizesInfoKHR const& info)
{
    return create_buffer(device,
        {
            .size = info.accelerationStructureSize,
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        });
}

vkrndr::buffer_t vkrndr::create_scratch_buffer(device_t const& device,
    VkAccelerationStructureBuildSizesInfoKHR const& info)
{
    return create_buffer(device,
        {
            .size = info.buildScratchSize,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        });
}

vkrndr::acceleration_structure_t vkrndr::create_acceleration_structure(
    device_t const& device,
    VkAccelerationStructureTypeKHR const type,
    VkAccelerationStructureBuildSizesInfoKHR const& info)
{
    acceleration_structure_t rv{
        .buffer = create_acceleration_structure_buffer(device, info)};

    VkAccelerationStructureCreateInfoKHR const create_info{
        .sType = vku::GetSType<VkAccelerationStructureCreateInfoKHR>(),
        .buffer = rv.buffer,
        .size = info.accelerationStructureSize,
        .type = type,
    };

    check_result(vkCreateAccelerationStructureKHR(device,
        &create_info,
        nullptr,
        &rv.handle));

    return rv;
}
