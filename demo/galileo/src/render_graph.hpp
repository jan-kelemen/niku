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
    struct [[nodiscard]] render_primitive_t final
    {
        uint32_t current_instance;
        uint32_t first_instance;
        uint32_t instance_count;

        VkPrimitiveTopology topology;

        uint32_t count;
        uint32_t first;

        bool is_indexed;
        int32_t vertex_offset;

        size_t material_index;
    };

    struct [[nodiscard]] render_mesh_t final
    {
        size_t first_primitive;
        size_t count;
    };

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

        void draw(VkCommandBuffer command_buffer);

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
        [[nodiscard]] size_t calculate_unique_draws(
            vkgltf::model_t const& model);

    private:
        vkrndr::backend_t* backend_;

        VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};

        vkrndr::buffer_t vertex_buffer_;
        vkrndr::buffer_t index_buffer_;

        vkgltf::model_t model_;
        std::vector<render_mesh_t> meshes_;
        std::vector<render_primitive_t> primitives_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace galileo
#endif
