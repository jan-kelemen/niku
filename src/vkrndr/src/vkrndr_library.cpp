#include <vkrndr_library.hpp>

#include <vkrndr_features.hpp>
#include <vkrndr_utility.hpp>

#include <volk.h>

#include <algorithm>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct vkrndr::library_handle_t final
{
    std::mutex available_instance_extensions_mtx;
    std::map<std::string, std::vector<VkExtensionProperties>, std::less<>>
        available_instance_extensions;
};

std::shared_ptr<vkrndr::library_handle_t> vkrndr::initialize()
{
    check_result(volkInitialize());

    // NOLINTBEGIN(cppcoreguidelines-owning-memory)
    return {new library_handle_t,
        [](library_handle_t* p)
        {
            delete p;
            volkFinalize();
        }};
    // NOLINTEND(cppcoreguidelines-owning-memory)
}

bool vkrndr::is_instance_extension_available(library_handle_t& handle,
    char const* const extension_name,
    char const* const layer_name)
{
    [[maybe_unused]] std::unique_lock const g{
        handle.available_instance_extensions_mtx};

    char const* const cache_layer_name{layer_name == nullptr ? "" : layer_name};

    auto it{handle.available_instance_extensions.find(cache_layer_name)};
    if (it == handle.available_instance_extensions.cend())
    {
        it = handle.available_instance_extensions
                 .emplace(cache_layer_name,
                     query_instance_extensions(layer_name))
                 .first;
    }

    return std::ranges::contains(it->second,
        std::string_view{extension_name},
        &VkExtensionProperties::extensionName);
}
