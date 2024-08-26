#include <vkrndr_imgui_render_layer.hpp>

#include <vkrndr_vulkan_commands.hpp>
#include <vkrndr_vulkan_context.hpp>
#include <vkrndr_vulkan_device.hpp>
#include <vkrndr_vulkan_queue.hpp>
#include <vkrndr_vulkan_swap_chain.hpp>
#include <vkrndr_vulkan_utility.hpp>
#include <vkrndr_vulkan_window.hpp>

#include <imgui_impl_vulkan.h>

#include <imgui.h>

#include <spdlog/spdlog.h>

#include <cassert>
#include <utility>

// IWYU pragma: no_include <span>

namespace
{
    [[nodiscard]] VkDescriptorPool create_descriptor_pool(
        vkrndr::vulkan_device const* const device)
    {
        VkDescriptorPoolSize imgui_sampler_pool_size{};
        imgui_sampler_pool_size.type =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        imgui_sampler_pool_size.descriptorCount = 1;

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = &imgui_sampler_pool_size;
        pool_info.maxSets = 1;

        VkDescriptorPool rv; // NOLINT
        vkrndr::check_result(
            vkCreateDescriptorPool(device->logical, &pool_info, nullptr, &rv));

        return rv;
    }

    void imgui_vulkan_result_callback(VkResult const result)
    {
        vkrndr::check_result(result);
    }
} // namespace

vkrndr::imgui_render_layer::imgui_render_layer(
    vkrndr::vulkan_window* const window,
    vkrndr::vulkan_context* const context,
    vkrndr::vulkan_device* const device,
    vkrndr::vulkan_swap_chain* const swap_chain)
    : window_{window}
    , device_{device}
    , descriptor_pool_{create_descriptor_pool(device)}
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); // NOLINT(readability-identifier-length)
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] =
        ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    ImGui::GetStyle().Colors[ImGuiCol_TitleBg] =
        ImVec4(0.27f, 0.27f, 0.54f, 1.00f);
    ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive] =
        ImVec4(0.32f, 0.32f, 0.63f, 1.00f);

    window->init_imgui();

    VkPipelineRenderingCreateInfoKHR rendering_create_info{};
    rendering_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_create_info.colorAttachmentCount = 1;
    VkFormat const image_format{swap_chain->image_format()};
    rendering_create_info.pColorAttachmentFormats = &image_format;

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = context->instance;
    init_info.PhysicalDevice = device->physical;
    init_info.Device = device->logical;
    init_info.QueueFamily = device->present_queue->family;
    init_info.Queue = device->present_queue->queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool_;
    init_info.RenderPass = VK_NULL_HANDLE;
    init_info.Subpass = 0;
    init_info.MinImageCount = swap_chain->min_image_count();
    init_info.ImageCount = swap_chain->image_count();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.CheckVkResultFn = imgui_vulkan_result_callback;
    init_info.UseDynamicRendering = true;
    init_info.PipelineRenderingCreateInfo = rendering_create_info;
    ImGui_ImplVulkan_Init(&init_info);
}

vkrndr::imgui_render_layer::~imgui_render_layer()
{
    ImGui_ImplVulkan_Shutdown();
    window_->shutdown_imgui();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(device_->logical, descriptor_pool_, nullptr);
}

void vkrndr::imgui_render_layer::begin_frame()
{
    assert(std::exchange(frame_rendered_, false));

    ImGui_ImplVulkan_NewFrame();
    window_->new_imgui_frame();
    ImGui::NewFrame();
}

void vkrndr::imgui_render_layer::draw(VkCommandBuffer command_buffer,
    VkImage target_image,
    VkImageView target_image_view,
    VkExtent2D extent)
{
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    check_result(vkBeginCommandBuffer(command_buffer, &begin_info));

    VkRenderingAttachmentInfo color_attachment_info{};

    color_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment_info.imageLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_info.imageView = target_image_view;

    VkRenderingInfo render_info{};
    render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    render_info.renderArea = {{0, 0}, extent};
    render_info.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
    render_info.layerCount = 1;
    render_info.colorAttachmentCount = 1;
    render_info.pColorAttachments = &color_attachment_info;

    wait_for_color_attachment_read(target_image, command_buffer);

    vkCmdBeginRendering(command_buffer, &render_info);

    render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);

    vkCmdEndRendering(command_buffer);

    transition_to_present_layout(target_image, command_buffer);

    check_result(vkEndCommandBuffer(command_buffer));
}

void vkrndr::imgui_render_layer::end_frame()
{
    if (!std::exchange(frame_rendered_, true))
    {
        spdlog::info("Ending ImGui frame without matching draw call");
        render();
    }
}

void vkrndr::imgui_render_layer::render()
{
    ImGui::Render();
    frame_rendered_ = true;
}
