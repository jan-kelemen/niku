#include <vkrndr_instance.hpp>

#include <vkrndr_error_code.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_utility.hpp>

#include <volk.h>

#include <algorithm>
#include <iterator>
#include <string_view>
#include <vector>

vkrndr::instance_t::~instance_t()
{
    vkDestroyInstance(handle, nullptr);
    handle = VK_NULL_HANDLE;
}

std::expected<vkrndr::instance_ptr_t, std::error_code> vkrndr::create_instance(
    instance_create_info_t const& create_info)
{
    auto const instance_version{create_info.maximum_vulkan_version.or_else(
        []() -> std::optional<uint32_t>
        {
            uint32_t rv;
            if (VkResult const result{vkEnumerateInstanceVersion(&rv)};
                is_success_result(result))
            {
                return rv;
            };

            return std::nullopt;
        })};
    if (!instance_version)
    {
        // This would be an extremely weird case
        return std::unexpected{vkrndr::make_error_code(VK_ERROR_UNKNOWN)};
    }

    std::vector<char const*> required_extensions{
        std::cbegin(create_info.extensions),
        std::cend(create_info.extensions)};

    std::ranges::copy_if(create_info.optional_extensions,
        std::back_inserter(required_extensions),
        std::bind_back(is_instance_extension_available, nullptr));

    VkApplicationInfo const app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = create_info.application_name,
        .applicationVersion = create_info.application_version,
        .pEngineName = "niku",
        .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .apiVersion = *instance_version};

    VkInstanceCreateInfo const ci{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = create_info.chain,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = count_cast(create_info.layers),
        .ppEnabledLayerNames = create_info.layers.data(),
        .enabledExtensionCount = count_cast(required_extensions),
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
