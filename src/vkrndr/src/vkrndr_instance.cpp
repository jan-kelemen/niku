#include <vkrndr_instance.hpp>

#include <vkrndr_error_code.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_library_handle.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/scope_fail.hpp>

#include <volk.h>

#include <algorithm>
#include <iterator>

// IWYU pragma: no_include <boost/scope/exception_checker.hpp>

vkrndr::instance_t::~instance_t()
{
    vkDestroyInstance(handle, nullptr);
    handle = VK_NULL_HANDLE;
}

std::expected<vkrndr::instance_ptr_t, std::error_code> vkrndr::create_instance(
    instance_create_info_t const& create_info)
{
    std::vector<char const*> required_extensions{
        std::cbegin(create_info.extensions),
        std::cend(create_info.extensions)};

#if VKRNDR_ENABLE_DEBUG_UTILS
    if (is_instance_extension_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#endif

    {
        auto const capabilities2{is_instance_extension_available(
            VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME)};

        auto const maintenance1{is_instance_extension_available(
            VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME)};

        if (capabilities2 && maintenance1)
        {
            required_extensions.push_back(
                VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            required_extensions.push_back(
                VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
        }
    }

    VkApplicationInfo const app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = create_info.application_name,
        .applicationVersion = create_info.application_version,
        .pEngineName = "niku",
        .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .apiVersion = create_info.maximum_vulkan_version};

    VkInstanceCreateInfo const ci{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = create_info.chain,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = count_cast(create_info.layers.size()),
        .ppEnabledLayerNames = create_info.layers.data(),
        .enabledExtensionCount = count_cast(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data()};

    instance_ptr_t rv{new instance_t};

    if (VkResult const result{vkCreateInstance(&ci, nullptr, &rv->handle)};
        result != VK_SUCCESS)
    {
        return std::unexpected{vkrndr::make_error_code(result)};
    }

    volkLoadInstanceOnly(rv->handle);

    std::ranges::copy(create_info.layers,
        std::inserter(rv->layers, rv->layers.begin()));

    std::ranges::copy(required_extensions,
        std::inserter(rv->extensions, rv->extensions.begin()));

    return rv;
}

bool vkrndr::is_instance_extension_available(char const* extension_name,
    char const* layer_name)
{
    return std::ranges::contains(query_instance_extensions(layer_name),
        std::string_view{extension_name},
        &VkExtensionProperties::extensionName);
}
