#include <materials.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>

#include <ngnast_scene_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_utility.hpp>

#include <volk.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <tuple>
#include <utility>

// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <type_traits>
// IWYU pragma: no_include <string_view>

namespace
{
    struct [[nodiscard]] material_t final
    {
        uint32_t base_color_texture_index{std::numeric_limits<uint32_t>::max()};
        float alpha_cutoff;
    };

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
        texture_uniform_binding.stageFlags =
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
            VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

        VkDescriptorSetLayoutBinding sampler_uniform_binding{};
        sampler_uniform_binding.binding = 1;
        sampler_uniform_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        sampler_uniform_binding.descriptorCount =
            cppext::narrow<uint32_t>(samplers);
        sampler_uniform_binding.stageFlags =
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
            VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

        VkDescriptorSetLayoutBinding material_uniform_binding{};
        material_uniform_binding.binding = 2;
        material_uniform_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        material_uniform_binding.descriptorCount = 1;
        material_uniform_binding.stageFlags =
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
            VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

        std::array const bindings{texture_uniform_binding,
            sampler_uniform_binding,
            material_uniform_binding};

        return vkrndr::create_descriptor_set_layout(device, bindings).value();
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

        vkUpdateDescriptorSets(device,
            vkrndr::count_cast(descriptor_writes),
            descriptor_writes.data(),
            0,
            nullptr);
    }

    [[nodiscard]] VkSampler create_sampler(vkrndr::device_t const& device,
        ngnast::sampler_info_t const& info,
        uint32_t const mip_levels)
    {
        VkPhysicalDeviceProperties properties; // NOLINT
        vkGetPhysicalDeviceProperties(device, &properties);

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
            vkCreateSampler(device, &sampler_info, nullptr, &rv));

        return rv;
    }

    [[nodiscard]] vkrndr::buffer_t create_material_uniform(
        vkrndr::backend_t& backend,
        std::span<ngnast::material_t const> const& materials)
    {
        auto const image_and_sampler = [](ngnast::texture_t const& texture)
        {
            return std::make_pair(cppext::narrow<uint32_t>(texture.image_index),
                cppext::narrow<uint32_t>(texture.sampler_index));
        };

        auto const to_gpu_material = [&image_and_sampler](
                                         ngnast::material_t const& m)
        {
            material_t rv{
                .alpha_cutoff = m.alpha_mode == ngnast::alpha_mode_t::mask
                    ? m.alpha_cutoff
                    : 0.0f,
            };

            if (auto const* const texture{
                    m.pbr_metallic_roughness.base_color_texture})
            {
                std::tie(rv.base_color_texture_index, std::ignore) =
                    image_and_sampler(*texture);
            }

            return rv;
        };

        vkrndr::buffer_t const staging_buffer{
            create_staging_buffer(backend.device(),
                sizeof(material_t) * materials.size())};
        vkrndr::mapped_memory_t staging_buffer_map{
            map_memory(backend.device(), staging_buffer)};
        material_t* const gpu_materials{staging_buffer_map.as<material_t>()};

        std::ranges::transform(materials, gpu_materials, to_gpu_material);
        unmap_memory(backend.device(), &staging_buffer_map);

        vkrndr::buffer_t rv{create_buffer(backend.device(),
            {.size = staging_buffer.size,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT})};
        VKRNDR_IF_DEBUG_UTILS(
            object_name(backend.device(), rv, "Materials Storage Buffer"));

        backend.transfer_buffer(staging_buffer, rv);

        destroy(backend.device(), staging_buffer);

        return rv;
    }
} // namespace

heatx::materials_t::materials_t(vkrndr::backend_t& backend) : backend_{&backend}
{
    create_dummy_material();
}

heatx::materials_t::~materials_t()
{
    clear();

    destroy(backend_->device(), dummy_uniform_);

    vkDestroySampler(backend_->device(), default_sampler_, nullptr);

    destroy(backend_->device(), white_pixel_);

    free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        cppext::as_span(dummy_descriptor_set_));

    vkDestroyDescriptorSetLayout(backend_->device(),
        dummy_descriptor_layout_,
        nullptr);
}

VkDescriptorSetLayout heatx::materials_t::descriptor_layout() const
{
    return has_materials_ ? descriptor_layout_ : dummy_descriptor_layout_;
}

void heatx::materials_t::load(ngnast::scene_model_t& model)
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

void heatx::materials_t::bind_on(VkCommandBuffer command_buffer,
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

void heatx::materials_t::create_dummy_material()
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
        create_sampler(backend_->device(), ngnast::sampler_info_t{}, 1);

    dummy_descriptor_layout_ =
        create_descriptor_set_layout(backend_->device(), 1, 1);

    ngnast::material_t const dummy_material;
    dummy_uniform_ =
        create_material_uniform(*backend_, cppext::as_span(dummy_material));

    vkrndr::check_result(allocate_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        cppext::as_span(dummy_descriptor_layout_),
        cppext::as_span(dummy_descriptor_set_)));

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

void heatx::materials_t::transfer_textures(ngnast::scene_model_t const& model)
{
    std::vector<VkDescriptorImageInfo> image_descriptors;
    std::vector<VkDescriptorImageInfo> sampler_descriptors;
    if (!model.images.empty())
    {
        uint32_t max_mip_levels{};
        images_.reserve(model.images.size());
        for (ngnast::image_t const& image : model.images)
        {
            images_.push_back(backend_->transfer_image(
                std::span{image.data.get(), image.data_size},
                image.extent,
                image.format,
                vkrndr::max_mip_levels(image.extent.width,
                    image.extent.height)));

            max_mip_levels =
                std::max(max_mip_levels, images_.back().mip_levels);

            image_descriptors.push_back(
                sampled_image_descriptor(images_.back()));
        }

        for (auto const& sampler : model.samplers)
        {
            samplers_.push_back(
                create_sampler(backend_->device(), sampler, max_mip_levels));

            sampler_descriptors.push_back(
                vkrndr::sampler_descriptor(samplers_.back()));
        }
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

    vkrndr::check_result(allocate_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        cppext::as_span(descriptor_layout_),
        cppext::as_span(descriptor_set_)));

    update_descriptor_set(backend_->device(),
        descriptor_set_,
        image_descriptors,
        sampler_descriptors,
        vkrndr::buffer_descriptor(uniform_));
}

void heatx::materials_t::clear()
{
    destroy(backend_->device(), uniform_);
    uniform_ = {};

    for (VkSampler sampler : samplers_)
    {
        vkDestroySampler(backend_->device(), sampler, nullptr);
    }
    samplers_.clear();

    for (vkrndr::image_t const& image : images_)
    {
        destroy(backend_->device(), image);
    }
    images_.clear();

    if (descriptor_set_ != VK_NULL_HANDLE)
    {
        free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(descriptor_set_));
        descriptor_set_ = VK_NULL_HANDLE;
    }

    vkDestroyDescriptorSetLayout(backend_->device(),
        descriptor_layout_,
        nullptr);
    descriptor_layout_ = VK_NULL_HANDLE;
}
