#include <vkrndr_vulkan_context.hpp>

#include <vkrndr_vulkan_utility.hpp>
#include <vkrndr_vulkan_window.hpp>

#include <vulkan/vk_platform.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <spdlog/common.h>
// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <string>

namespace
{
    constexpr std::array const validation_layers{"VK_LAYER_KHRONOS_validation"};

    constexpr std::array const validation_feature_enable{
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};

    constexpr VkValidationFeaturesEXT validation_features = {
        VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
        nullptr,
        vkrndr::count_cast(validation_feature_enable.size()),
        validation_feature_enable.data(),
        0,
        nullptr};

    [[nodiscard]] bool check_validation_layer_support()
    {
        uint32_t count{};
        vkEnumerateInstanceLayerProperties(&count, nullptr);

        std::vector<VkLayerProperties> available_layers{count};
        vkEnumerateInstanceLayerProperties(&count, available_layers.data());

        // cppcheck-suppress-begin useStlAlgorithm
        for (std::string_view const layer_name : validation_layers)
        {
            if (!std::ranges::any_of(available_layers,
                    [layer_name](VkLayerProperties const& layer) noexcept
                    {
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                        return layer_name == layer.layerName;
                    }))
            {
                return false;
            }
        }
        // cppcheck-suppress-end useStlAlgorithm

        return true;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT const severity,
        [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT const type,
        VkDebugUtilsMessengerCallbackDataEXT const* const callback_data,
        [[maybe_unused]] void* const user_data)
    {
        switch (severity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            spdlog::debug(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            spdlog::info(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            spdlog::warn(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            spdlog::error(callback_data->pMessage);
            break;
        default:
            spdlog::error("Unrecognized severity {}. {}",
                std::to_underlying(severity),
                callback_data->pMessage);
            break;
        }

        return VK_FALSE;
    }

    void populate_debug_messenger_create_info(
        VkDebugUtilsMessengerCreateInfoEXT& info)
    {
        info = {};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        info.pfnUserCallback = debug_callback;
    }

    [[nodiscard]] VkResult create_debug_utils_messenger_ext(
        VkInstance const instance,
        VkDebugUtilsMessengerCreateInfoEXT const* const create_info,
        VkAllocationCallbacks const* const allocator,
        VkDebugUtilsMessengerEXT* const debug_messenger)
    {
        // NOLINTNEXTLINE
        auto const func{reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"))};

        if (func != nullptr)
        {
            return func(instance, create_info, allocator, debug_messenger);
        }

        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    void destroy_debug_utils_messenger_ext(VkInstance const instance,
        VkDebugUtilsMessengerEXT const debug_messenger,
        VkAllocationCallbacks const* allocator)
    {
        // NOLINTNEXTLINE
        auto const func{reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance,
                "vkDestroyDebugUtilsMessengerEXT"))};

        if (func != nullptr)
        {
            func(instance, debug_messenger, allocator);
        }
    }

    [[nodiscard]] VkResult create_debug_messenger(VkInstance const instance,
        VkDebugUtilsMessengerEXT& debug_messenger)
    {
        VkDebugUtilsMessengerCreateInfoEXT create_info{};
        populate_debug_messenger_create_info(create_info);

        return create_debug_utils_messenger_ext(instance,
            &create_info,
            nullptr,
            &debug_messenger);
    }
} // namespace

vkrndr::vulkan_context vkrndr::create_context(
    vkrndr::vulkan_window const* const window,
    bool const setup_validation_layers)
{
    vulkan_context rv;

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "vkrndr";
    app_info.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    std::vector<char const*> required_extensions{window->required_extensions()};

    bool has_debug_utils_extension{setup_validation_layers};
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
    populate_debug_messenger_create_info(debug_create_info);
    if (setup_validation_layers)
    {
        if (check_validation_layer_support())
        {
            create_info.enabledLayerCount =
                count_cast(validation_layers.size());
            create_info.ppEnabledLayerNames = validation_layers.data();

            required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            debug_create_info.pNext = &validation_features;
            create_info.pNext = &debug_create_info;
        }
        else
        {
            spdlog::warn("Validation layers requested but not available!");
            has_debug_utils_extension = false;
        }
    }

    create_info.enabledExtensionCount = count_cast(required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();

    check_result(vkCreateInstance(&create_info, nullptr, &rv.instance));

    if (has_debug_utils_extension)
    {
        check_result(create_debug_messenger(rv.instance, rv.debug_messenger));
    }

    check_result(window->create_surface(rv.instance, rv.surface));

    return rv;
}

void vkrndr::destroy(vulkan_context* const context)
{
    if (context)
    {
        vkDestroySurfaceKHR(context->instance, context->surface, nullptr);

        destroy_debug_utils_messenger_ext(context->instance,
            context->debug_messenger,
            nullptr);

        vkDestroyInstance(context->instance, nullptr);
    }
}
