#include <vkrndr_context.hpp>

#include <vkrndr_utility.hpp>

#include <volk.h>

vkrndr::context_t vkrndr::create_context(
    context_create_info_t const& create_info)
{
    VkApplicationInfo const app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = create_info.application_name,
        .applicationVersion = create_info.application_version,
        .pEngineName = "niku",
        .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .apiVersion = create_info.minimal_vulkan_version};

    VkInstanceCreateInfo const ci{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = create_info.chain,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = count_cast(create_info.layers.size()),
        .ppEnabledLayerNames = create_info.layers.data(),
        .enabledExtensionCount = count_cast(create_info.extensions.size()),
        .ppEnabledExtensionNames = create_info.extensions.data()};

    context_t rv;
    check_result(vkCreateInstance(&ci, nullptr, &rv.instance));

    volkLoadInstance(rv.instance);

    return rv;
}

void vkrndr::destroy(context_t* const context)
{
    if (context)
    {
        vkDestroyInstance(context->instance, nullptr);
    }
}
