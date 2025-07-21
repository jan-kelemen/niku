#include <vkrndr_instance.hpp>

#include <vkrndr_library.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/scope_fail.hpp>

#include <volk.h>

#include <algorithm>
#include <iterator>

// IWYU pragma: no_include <boost/scope/exception_checker.hpp>
namespace
{
    void destroy(vkrndr::instance_t* const instance)
    {
        vkDestroyInstance(instance->handle, nullptr);
        instance->layers.clear();
        instance->extensions.clear();
    }
} // namespace

std::shared_ptr<vkrndr::instance_t> vkrndr::create_instance(
    library_handle_t const& library_handle,
    instance_create_info_t const& create_info)
{
    std::vector<char const*> required_extensions{
        std::cbegin(create_info.extensions),
        std::cend(create_info.extensions)};

#if VKRNDR_ENABLE_DEBUG_UTILS
    if (is_instance_extension_available(library_handle,
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#endif

    {
        auto const capabilities2{is_instance_extension_available(library_handle,
            VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME)};

        auto const maintenance1{is_instance_extension_available(library_handle,
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

    std::shared_ptr<instance_t> rv{new instance_t, destroy};

    check_result(vkCreateInstance(&ci, nullptr, &rv->handle));

    volkLoadInstanceOnly(rv->handle);

    std::ranges::copy(create_info.layers,
        std::inserter(rv->layers, rv->layers.begin()));

    std::ranges::copy(required_extensions,
        std::inserter(rv->extensions, rv->extensions.begin()));

    return rv;
}
