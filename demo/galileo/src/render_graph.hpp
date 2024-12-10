#ifndef GALILEO_RENDER_GRAPH_INCLUDED
#define GALILEO_RENDER_GRAPH_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>

#include <glm/mat4x4.hpp>

#include <volk.h>

#include <cstddef>
#include <span>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace galileo
{
    class [[nodiscard]] render_graph_t final
    {
    public:
        [[nodiscard]] static std::span<VkVertexInputBindingDescription const>
        binding_description();

        [[nodiscard]] static std::span<VkVertexInputAttributeDescription const>
        attribute_description();

    public:
        explicit render_graph_t(vkrndr::backend_t& backend);

        render_graph_t(render_graph_t const&) = delete;

        render_graph_t(render_graph_t&&) noexcept = delete;

    public:
        ~render_graph_t();

    public:
        [[nodiscard]] VkDescriptorSetLayout descriptor_set_layout() const;

        void consume(vkgltf::model_t&& model);

        void begin_frame();

        void update(size_t index, glm::mat4 const& position);

        void bind_on(VkCommandBuffer command_buffer,
            VkPipelineLayout layout,
            VkPipelineBindPoint bind_point);

        void draw_node(size_t index, VkCommandBuffer command_buffer) const;

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
        vkrndr::backend_t* backend_;

        VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};

        vkrndr::buffer_t vertex_buffer_;
        vkrndr::buffer_t index_buffer_;

        vkgltf::model_t model_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace galileo
#endif
