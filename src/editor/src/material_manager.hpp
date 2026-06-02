#ifndef EDITOR_MATERIAL_MANAGER_INCLUDED
#define EDITOR_MATERIAL_MANAGER_INCLUDED

#include <cppext_hash_adapter.hpp>
#include <cppext_numeric.hpp>

#include <vkrndr_device.hpp> // IWYU pragma: keep
#include <vkrndr_error_code.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_sampler.hpp>
#include <vkrndr_utility.hpp>

#include <boost/hash2/md5.hpp>
#include <boost/unordered/unordered_map.hpp>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <volk.h> // IWYU pragma: keep

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <ranges>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

// IWYU pragma: no_include <boost/core/allocator_access.hpp>
// IWYU pragma: no_include <boost/unordered/unordered_map_fwd.hpp>
// IWYU pragma: no_forward_declare vkrndr::device_t

namespace ngnast
{
    struct image_t;
    struct material_t;
} // namespace ngnast

namespace ngnwsi
{
    class imgui_layer_t;
} // namespace ngnwsi

namespace vkrndr
{
    class execution_port_t;
} // namespace vkrndr

namespace editor::detail
{
    using sampler_hash_t = cppext::hash_adapter_t<vkrndr::sampler_properties_t,
        boost::hash2::md5_128>;

    using sampler_index_map_t = boost::unordered::
        unordered_map<vkrndr::sampler_properties_t, uint32_t, sampler_hash_t>;
} // namespace editor::detail

namespace editor
{
    struct [[nodiscard]] texture_t final
    {
        uint32_t image_index;
        uint32_t sampler_index;
    };

    struct [[nodiscard]] material_t final
    {
        glm::vec4 base_color_factor;
        glm::vec3 emissive_factor;
        uint32_t base_color_texture_index{std::numeric_limits<uint32_t>::max()};
        uint32_t base_color_sampler_index{std::numeric_limits<uint32_t>::max()};
        float alpha_cutoff;
        uint32_t metallic_roughness_texture_index{
            std::numeric_limits<uint32_t>::max()};
        uint32_t metallic_roughness_sampler_index{
            std::numeric_limits<uint32_t>::max()};
        float metallic_factor;
        float roughness_factor;
        float occlusion_strength;
        uint32_t normal_texture_index{std::numeric_limits<uint32_t>::max()};
        uint32_t normal_sampler_index{std::numeric_limits<uint32_t>::max()};
        uint32_t emissive_texture_index{std::numeric_limits<uint32_t>::max()};
        uint32_t emissive_sampler_index{std::numeric_limits<uint32_t>::max()};
        uint32_t occlusion_texture_index{std::numeric_limits<uint32_t>::max()};
        uint32_t occlusion_sampler_index{std::numeric_limits<uint32_t>::max()};
        float normal_scale;
        uint32_t double_sided{};
        float emissive_strength;
    };

    struct [[nodiscard]] material_manager_t final
    {
        std::vector<vkrndr::image_t> images;
        std::vector<VkSampler> samplers;
        detail::sampler_index_map_t sampler_lookup;

        std::vector<texture_t> textures;
        std::vector<material_t> materials;
    };

    struct [[nodiscard]] material_manager_ui_t final
    {
        size_t displayed_texture_index{};
        std::vector<std::pair<uint32_t, VkDescriptorSet>> image_descriptors;

        size_t displayed_material_index{};
    };

    [[nodiscard]] std::expected<material_manager_t, std::error_code>
    create_material_manager(vkrndr::device_t const& device);

    void destroy(vkrndr::device_t const& device,
        material_manager_t const& materials);

    [[nodiscard]] std::expected<std::vector<vkrndr::image_t>, std::error_code>
    transfer_images(vkrndr::device_t const& device,
        vkrndr::execution_port_t& transfer_queue,
        vkrndr::execution_port_t& graphics_queue,
        std::span<ngnast::image_t> const& images);

    // NOLINTBEGIN(cppcoreguidelines-missing-std-forward)
    template<typename Range>
    [[nodiscard]] std::vector<uint32_t> add_images(entt::handle manager_entity,
        Range&& images)
    requires(std::ranges::input_range<Range>)
    {
        auto& manager{manager_entity.get<material_manager_t>()};

        std::vector<uint32_t> rv;
        if constexpr (std::ranges::sized_range<Range>)
        {
            size_t const size{std::ranges::size(images)};
            rv.reserve(size);
            manager.images.reserve(manager.images.size() + size);
        }

        for (auto&& image : images)
        {
            manager.images.push_back(std::forward<decltype(image)>(image));
            rv.push_back(cppext::narrow<uint32_t>(manager.images.size() - 1));
        }

        return rv;
    }

    // NOLINTEND(cppcoreguidelines-missing-std-forward)

    // NOLINTBEGIN(cppcoreguidelines-missing-std-forward)
    template<typename Range>
    [[nodiscard]] std::expected<std::vector<uint32_t>, std::error_code>
    add_samplers(entt::handle manager_entity,
        vkrndr::device_t const& device,
        Range&& sampler_properties)
    requires(std::ranges::input_range<Range>)
    {
        std::expected<std::vector<uint32_t>, std::error_code> rv;

        std::vector<uint32_t> indices;
        if constexpr (std::ranges::sized_range<Range>)
        {
            indices.reserve(std::ranges::size(sampler_properties));
        }

        auto& manager{manager_entity.get<material_manager_t>()};
        for (auto&& properties : sampler_properties)
        {
            auto const& [it, inserted] = manager.sampler_lookup.try_emplace(
                std::forward<decltype(properties)>(properties),
                cppext::narrow<uint32_t>(manager.samplers.size()));
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

    [[nodiscard]] uint32_t add_texture(entt::handle manager_entity,
        uint32_t sampler_index,
        uint32_t image_index);

    [[nodiscard]] uint32_t add_material(entt::handle manager_entity,
        ngnast::material_t const& asset_material,
        std::span<uint32_t> const& texture_remap_table);

    void draw_material_manager(entt::handle manager_entity,
        ngnwsi::imgui_layer_t& imgui);
} // namespace editor

#endif
