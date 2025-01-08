#include <materials.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>
#include <cppext_pragma_warning.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_utility.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <volk.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <span>
#include <tuple>
#include <utility>

// IWYU pragma: no_include <type_traits>

namespace
{
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

    static_assert(sizeof(material_t) % 16 == 0);

    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        vkrndr::device_t const& device,
        size_t const images,
        size_t const samplers)
    {
        VkDescriptorSetLayoutBinding texture_uniform_binding{};
        texture_uniform_binding.binding = 0;
        texture_uniform_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texture_uniform_binding.descriptorCount =
            cppext::narrow<uint32_t>(images);
        texture_uniform_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding sampler_uniform_binding{};
        sampler_uniform_binding.binding = 1;
        sampler_uniform_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        sampler_uniform_binding.descriptorCount =
            cppext::narrow<uint32_t>(samplers);
        sampler_uniform_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding material_uniform_binding{};
        material_uniform_binding.binding = 2;
        material_uniform_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        material_uniform_binding.descriptorCount = 1;
        material_uniform_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array const bindings{texture_uniform_binding,
            sampler_uniform_binding,
            material_uniform_binding};

        return vkrndr::create_descriptor_set_layout(device, bindings);
    }

    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        std::span<VkDescriptorImageInfo const> const& textures_info,
        std::span<VkDescriptorImageInfo const> const& samplers_info,
        VkDescriptorBufferInfo const material_info)
    {
        VkWriteDescriptorSet texture_uniform_write{};
        texture_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texture_uniform_write.dstSet = descriptor_set;
        texture_uniform_write.dstBinding = 0;
        texture_uniform_write.dstArrayElement = 0;
        texture_uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texture_uniform_write.descriptorCount =
            cppext::narrow<uint32_t>(textures_info.size());
        texture_uniform_write.pImageInfo = textures_info.data();

        VkWriteDescriptorSet sampler_uniform_write{};
        sampler_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sampler_uniform_write.dstSet = descriptor_set;
        sampler_uniform_write.dstBinding = 1;
        sampler_uniform_write.dstArrayElement = 0;
        sampler_uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        sampler_uniform_write.descriptorCount =
            cppext::narrow<uint32_t>(samplers_info.size());
        sampler_uniform_write.pImageInfo = samplers_info.data();

        VkWriteDescriptorSet material_uniform_write{};
        material_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        material_uniform_write.dstSet = descriptor_set;
        material_uniform_write.dstBinding = 2;
        material_uniform_write.dstArrayElement = 0;
        material_uniform_write.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        material_uniform_write.descriptorCount = 1;
        material_uniform_write.pBufferInfo = &material_info;

        std::array const descriptor_writes{texture_uniform_write,
            sampler_uniform_write,
            material_uniform_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }

    [[nodiscard]] VkSampler create_sampler(vkrndr::device_t const& device,
        vkgltf::sampler_info_t const& info,
        uint32_t const mip_levels)
    {
        VkPhysicalDeviceProperties properties; // NOLINT
        vkGetPhysicalDeviceProperties(device.physical, &properties);

        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = info.mag_filter;
        sampler_info.minFilter = info.min_filter;
        sampler_info.addressModeU = info.wrap_u;
        sampler_info.addressModeV = info.wrap_v;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_NEVER;
        sampler_info.mipmapMode = info.mipmap_mode;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = cppext::as_fp(mip_levels);

        VkSampler rv; // NOLINT
        vkrndr::check_result(
            vkCreateSampler(device.logical, &sampler_info, nullptr, &rv));

        return rv;
    }

    [[nodiscard]] vkrndr::buffer_t create_material_uniform(
        vkrndr::backend_t& backend,
        std::span<vkgltf::material_t const> const& materials)
    {
        auto const image_and_sampler = [](vkgltf::texture_t const& texture)
        {
            return std::make_pair(cppext::narrow<uint32_t>(texture.image_index),
                cppext::narrow<uint32_t>(texture.sampler_index));
        };

        auto const to_gpu_material = [&image_and_sampler](
                                         vkgltf::material_t const& m)
        {
            DISABLE_WARNING_PUSH
            DISABLE_WARNING_MISSING_FIELD_INITIALIZERS

            material_t rv{
                .base_color_factor = m.pbr_metallic_roughness.base_color_factor,
                .emissive_factor = m.emissive_factor,
                .alpha_cutoff = m.alpha_mode == vkgltf::alpha_mode_t::mask
                    ? m.alpha_cutoff
                    : 0.0f,
                .metallic_factor = m.pbr_metallic_roughness.metallic_factor,
                .roughness_factor = m.pbr_metallic_roughness.roughness_factor,
                .occlusion_strength = m.occlusion_strength,
                .normal_scale = m.normal_scale,
                .double_sided = static_cast<uint32_t>(m.double_sided),
                .emissive_strength = m.emissive_strength};

            DISABLE_WARNING_POP

            if (auto const* const texture{
                    m.pbr_metallic_roughness.base_color_texture})
            {
                std::tie(rv.base_color_texture_index,
                    rv.base_color_sampler_index) = image_and_sampler(*texture);
            }

            if (auto const* const texture{
                    m.pbr_metallic_roughness.metallic_roughness_texture})
            {
                std::tie(rv.metallic_roughness_texture_index,
                    rv.metallic_roughness_sampler_index) =
                    image_and_sampler(*texture);
            }

            if (auto const* const texture{m.normal_texture})
            {
                std::tie(rv.normal_texture_index, rv.normal_sampler_index) =
                    image_and_sampler(*texture);
            }

            if (auto const* const texture{m.emissive_texture})
            {
                std::tie(rv.emissive_texture_index, rv.emissive_sampler_index) =
                    image_and_sampler(*texture);
            }

            if (auto const* const texture{m.occlusion_texture})
            {
                std::tie(rv.occlusion_texture_index,
                    rv.occlusion_sampler_index) = image_and_sampler(*texture);
            }

            return rv;
        };

        vkrndr::buffer_t staging_buffer{create_staging_buffer(backend.device(),
            sizeof(material_t) * materials.size())};
        vkrndr::mapped_memory_t staging_buffer_map{
            map_memory(backend.device(), staging_buffer)};
        material_t* const gpu_materials{staging_buffer_map.as<material_t>()};

        std::ranges::transform(materials, gpu_materials, to_gpu_material);
        unmap_memory(backend.device(), &staging_buffer_map);

        vkrndr::buffer_t rv{create_buffer(backend.device(),
            staging_buffer.size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
        object_name(backend.device(), rv, "Materials Storage Buffer");

        backend.transfer_buffer(staging_buffer, rv);

        destroy(&backend.device(), &staging_buffer);

        return rv;
    }
} // namespace

gltfviewer::materials_t::materials_t(vkrndr::backend_t& backend)
    : backend_{&backend}
{
    create_dummy_material();
}

gltfviewer::materials_t::~materials_t()
{
    clear();

    destroy(&backend_->device(), &dummy_uniform_);

    vkDestroySampler(backend_->device().logical, default_sampler_, nullptr);

    destroy(&backend_->device(), &white_pixel_);

    vkrndr::free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        cppext::as_span(dummy_descriptor_set_));

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        dummy_descriptor_layout_,
        nullptr);
}

VkDescriptorSetLayout gltfviewer::materials_t::descriptor_layout() const
{
    return has_materials_ ? descriptor_layout_ : dummy_descriptor_layout_;
}

void gltfviewer::materials_t::load(vkgltf::model_t& model)
{
    clear();

    has_materials_ = !model.materials.empty();
    if (has_materials_)
    {
        uniform_ = create_material_uniform(*backend_, model.materials);
        transfer_textures(model);
    }

    model.textures.clear();
    model.samplers.clear();
}

void gltfviewer::materials_t::bind_on(VkCommandBuffer command_buffer,
    VkPipelineLayout const layout,
    VkPipelineBindPoint const bind_point)
{
    vkCmdBindDescriptorSets(command_buffer,
        bind_point,
        layout,
        1,
        1,
        has_materials_ ? &descriptor_set_ : &dummy_descriptor_set_,
        0,
        nullptr);
}

void gltfviewer::materials_t::create_dummy_material()
{
    static constexpr std::array white{
        static_cast<std::byte>(255),
        static_cast<std::byte>(255),
        static_cast<std::byte>(255),
        static_cast<std::byte>(255),
    };

    white_pixel_ = backend_->transfer_image(white,
        VkExtent2D{1, 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        1);

    default_sampler_ =
        create_sampler(backend_->device(), vkgltf::sampler_info_t{}, 1);

    dummy_descriptor_layout_ =
        create_descriptor_set_layout(backend_->device(), 1, 1);

    vkgltf::material_t const dummy_material;
    dummy_uniform_ =
        create_material_uniform(*backend_, cppext::as_span(dummy_material));

    vkrndr::create_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        cppext::as_span(dummy_descriptor_layout_),
        cppext::as_span(dummy_descriptor_set_));

    VkDescriptorImageInfo const image_descriptor{
        vkrndr::sampled_image_descriptor(white_pixel_)};

    VkDescriptorImageInfo const sampler_descriptor{
        vkrndr::sampler_descriptor(default_sampler_)};

    update_descriptor_set(backend_->device(),
        dummy_descriptor_set_,
        cppext::as_span(image_descriptor),
        cppext::as_span(sampler_descriptor),
        vkrndr::buffer_descriptor(dummy_uniform_));
}

void gltfviewer::materials_t::transfer_textures(vkgltf::model_t& model)
{
    std::vector<VkDescriptorImageInfo> image_descriptors;
    std::vector<VkDescriptorImageInfo> sampler_descriptors;
    if (!model.images.empty())
    {
        // clang-format off
        uint32_t const max_mip_levels{std::ranges::max(model.images,
            std::less{},
            &vkrndr::image_t::mip_levels).mip_levels};
        // clang-format on

        // cppcheck-suppress-begin useStlAlgorithm
        for (auto const& sampler : model.samplers)
        {
            samplers_.push_back(
                create_sampler(backend_->device(), sampler, max_mip_levels));
        }
        // cppcheck-suppress-end useStlAlgorithm

        image_descriptors.reserve(model.images.size());
        std::ranges::transform(model.images,
            std::back_inserter(image_descriptors),
            [](vkrndr::image_t const& image)
            { return vkrndr::sampled_image_descriptor(image); });

        sampler_descriptors.reserve(model.samplers.size());
        std::ranges::transform(samplers_,
            std::back_inserter(sampler_descriptors),
            vkrndr::sampler_descriptor);
    }
    else
    {
        image_descriptors.push_back(
            vkrndr::sampled_image_descriptor(white_pixel_));
        sampler_descriptors.push_back(
            vkrndr::sampler_descriptor(default_sampler_));
    }

    descriptor_layout_ = create_descriptor_set_layout(backend_->device(),
        image_descriptors.size(),
        sampler_descriptors.size());

    vkrndr::create_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        cppext::as_span(descriptor_layout_),
        cppext::as_span(descriptor_set_));

    update_descriptor_set(backend_->device(),
        descriptor_set_,
        image_descriptors,
        sampler_descriptors,
        vkrndr::buffer_descriptor(uniform_));

    images_ = std::move(model.images);
}

void gltfviewer::materials_t::clear()
{
    destroy(&backend_->device(), &uniform_);
    uniform_ = {};

    for (VkSampler sampler : samplers_)
    {
        vkDestroySampler(backend_->device().logical, sampler, nullptr);
    }
    samplers_.clear();

    for (vkrndr::image_t& image : images_)
    {
        destroy(&backend_->device(), &image);
    }
    images_.clear();

    if (descriptor_set_ != VK_NULL_HANDLE)
    {
        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(descriptor_set_));
        descriptor_set_ = VK_NULL_HANDLE;
    }

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        descriptor_layout_,
        nullptr);
    descriptor_layout_ = VK_NULL_HANDLE;
}
