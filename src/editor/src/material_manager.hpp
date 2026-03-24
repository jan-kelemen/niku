#ifndef EDITOR_MATERIAL_MANAGER_INCLUDED
#define EDITOR_MATERIAL_MANAGER_INCLUDED

#include <cppext_hash_adapter.hpp>

#include <vkrndr_device.hpp> // IWYU pragma: keep
#include <vkrndr_error_code.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_sampler.hpp>
#include <vkrndr_utility.hpp>

#include <boost/hash2/md5.hpp>
#include <boost/unordered/unordered_map.hpp>

#include <volk.h> // IWYU pragma: keep

#include <cstddef>
#include <expected>
#include <ranges>
#include <system_error>
#include <utility>
#include <vector>

// IWYU pragma: no_include <boost/core/allocator_access.hpp>
// IWYU pragma: no_include <boost/unordered/unordered_map_fwd.hpp>
// IWYU pragma: no_forward_declare vkrndr::device_t

namespace editor::detail
{
    using sampler_hash_t = cppext::hash_adapter_t<vkrndr::sampler_properties_t,
        boost::hash2::md5_128>;

    using sampler_index_map_t = boost::unordered::
        unordered_map<vkrndr::sampler_properties_t, size_t, sampler_hash_t>;
} // namespace editor::detail

namespace editor
{
    struct [[nodiscard]] material_manager_t final
    {
        std::vector<vkrndr::image_t> images;
        std::vector<VkSampler> samplers;
        detail::sampler_index_map_t sampler_lookup;
    };

    [[nodiscard]] std::expected<material_manager_t, std::error_code>
    create_material_manager(vkrndr::device_t const& device);

    void destroy(vkrndr::device_t const& device,
        material_manager_t const& materials);

    // NOLINTBEGIN(cppcoreguidelines-missing-std-forward)
    template<typename Range>
    [[nodiscard]] std::vector<size_t> add_images(material_manager_t& manager,
        Range&& images)
    requires(std::ranges::input_range<Range>)
    {
        std::vector<size_t> rv;
        if constexpr (std::ranges::sized_range<Range>)
        {
            size_t const size{std::ranges::size(images)};
            rv.reserve(size);
            manager.images.reserve(manager.images.size() + size);
        }

        for (auto&& image : images)
        {
            manager.images.push_back(std::forward<decltype(image)>(image));
            rv.push_back(manager.images.size() - 1);
        }

        return rv;
    }

    // NOLINTEND(cppcoreguidelines-missing-std-forward)

    // NOLINTBEGIN(cppcoreguidelines-missing-std-forward)
    template<typename Range>
    [[nodiscard]] std::expected<std::vector<size_t>, std::error_code>
    add_samplers(material_manager_t& manager,
        vkrndr::device_t const& device,
        Range&& sampler_properties)
    requires(std::ranges::input_range<Range>)
    {
        std::expected<std::vector<size_t>, std::error_code> rv;

        std::vector<size_t> indices;
        if constexpr (std::ranges::sized_range<Range>)
        {
            indices.reserve(std::ranges::size(sampler_properties));
        }

        for (auto&& properties : sampler_properties)
        {
            auto const& [it, inserted] = manager.sampler_lookup.try_emplace(
                std::forward<decltype(properties)>(properties),
                manager.samplers.size());
            if (inserted)
            {
                VkSamplerCreateInfo const ci{
                    as_create_info(device, it->first, false)};
                VkSampler sampler{VK_NULL_HANDLE};
                if (VkResult const result{
                        vkCreateSampler(device, &ci, nullptr, &sampler)};
                    !vkrndr::is_success_result(result))
                {
                    manager.sampler_lookup.erase(it);

                    rv = std::unexpected{vkrndr::make_error_code(result)};
                    return rv;
                }

                manager.samplers.push_back(sampler);
            }

            indices.push_back(it->second);
        }

        rv = std::move(indices);

        return rv;
    }

    // NOLINTEND(cppcoreguidelines-missing-std-forward)
} // namespace editor

#endif
