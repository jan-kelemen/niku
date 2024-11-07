#ifndef GLTFVIEWER_RENDER_GRAPH_INCLUDED
#define GLTFVIEWER_RENDER_GRAPH_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>

#include <volk.h>

#include <cstdint>
#include <functional>
#include <span>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace gltfviewer
{
    class [[nodiscard]] render_graph_t final
    {
    public:
        explicit render_graph_t(vkrndr::backend_t& backend);

        render_graph_t(render_graph_t const&) = delete;

        render_graph_t(render_graph_t&&) noexcept = delete;

    public:
        ~render_graph_t();

    public:
        [[nodiscard]] vkgltf::model_t const& model() const;

        [[nodiscard]] VkDescriptorSetLayout descriptor_layout() const;

        [[nodiscard]] std::span<VkVertexInputBindingDescription const>
        binding_description() const;

        [[nodiscard]] std::span<VkVertexInputAttributeDescription const>
        attribute_description() const;

        void load(vkgltf::model_t&& model);

        void bind_on(VkCommandBuffer command_buffer,
            VkPipelineLayout layout,
            VkPipelineBindPoint bind_point);

        void traverse(vkgltf::alpha_mode_t alpha_mode,
            VkCommandBuffer command_buffer,
            VkPipelineLayout layout,
            std::function<void(vkgltf::alpha_mode_t, bool)> const&
                switch_pipeline) const;

    public:
        render_graph_t& operator=(render_graph_t const&) = delete;

        render_graph_t& operator=(render_graph_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_data_t final
        {
            vkrndr::buffer_t uniform;
            vkrndr::mapped_memory_t uniform_map;
            VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
        };

    private:
        static void calculate_transforms(vkgltf::model_t const& model,
            std::span<frame_data_t> frames);

        uint32_t draw_node(VkCommandBuffer command_buffer,
            VkPipelineLayout layout,
            vkgltf::node_t const& node,
            vkgltf::alpha_mode_t const& alpha_mode,
            uint32_t index,
            std::function<void(vkgltf::alpha_mode_t, bool)> const&
                switch_pipeline) const;

        void clear();

    private:
        vkrndr::backend_t* backend_;

        VkDescriptorSetLayout descriptor_layout_{VK_NULL_HANDLE};

        vkgltf::model_t model_;

        uint32_t vertex_count_{};
        vkrndr::buffer_t vertex_buffer_;

        uint32_t index_count_{};
        vkrndr::buffer_t index_buffer_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace gltfviewer

#endif
