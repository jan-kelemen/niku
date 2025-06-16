#include <vkrndr_instance.hpp>

#include <vkrndr_utility.hpp>

#include <boost/scope/scope_fail.hpp>

#include <volk.h>

#include <algorithm>
#include <iterator>

// IWYU pragma: no_include <boost/scope/exception_checker.hpp>

vkrndr::instance_t vkrndr::create_instance(
    instance_create_info_t const& create_info)
{
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
        .enabledExtensionCount = count_cast(create_info.extensions.size()),
        .ppEnabledExtensionNames = create_info.extensions.data()};

    instance_t rv;
    check_result(vkCreateInstance(&ci, nullptr, &rv.handle));
    [[maybe_unused]] boost::scope::scope_fail const f{
        [&rv]() { destroy(&rv); }};

    volkLoadInstanceOnly(rv.handle);

    std::ranges::copy(create_info.layers,
        std::inserter(rv.layers, rv.layers.begin()));

    std::ranges::copy(create_info.extensions,
        std::inserter(rv.extensions, rv.extensions.begin()));

    return rv;
}

void vkrndr::destroy(instance_t* const instance)
{
    if (instance)
    {
        vkDestroyInstance(instance->handle, nullptr);
        instance->layers.clear();
        instance->extensions.clear();
    }
}
