#include <ngnwsi_imgui_layer.hpp>

#include <ngnwsi_sdl_window.hpp>

#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_swapchain.hpp>
#include <vkrndr_utility.hpp>

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <imgui.h>

#include <cassert>
#include <utility>

// IWYU pragma: no_include <string_view>

namespace
{
    [[nodiscard]] VkDescriptorPool create_descriptor_pool(
        vkrndr::device_t const& device)
    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_size.descriptorCount = 1;

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = &pool_size;
        pool_info.maxSets = 1;

        VkDescriptorPool rv; // NOLINT
        vkrndr::check_result(
            vkCreateDescriptorPool(device, &pool_info, nullptr, &rv));

        return rv;
    }

    void imgui_vulkan_result_callback(VkResult const result)
    {
        vkrndr::check_result(result);
    }

    PFN_vkVoidFunction load_vulkan_function(char const* const function,
        void* const user_data)
    {
        return vkGetInstanceProcAddr(static_cast<VkInstance>(user_data),
            function);
    }
} // namespace

ngnwsi::imgui_layer_t::imgui_layer_t(sdl_window_t const& window,
    vkrndr::instance_t const& instance,
    vkrndr::device_t& device,
    vkrndr::swapchain_t const& swapchain,
    vkrndr::execution_port_t const& present_queue)
    : device_{&device}
    , descriptor_pool_{create_descriptor_pool(device)}
    , context_{ImGui::CreateContext()}
{
    IMGUI_CHECKVERSION();

    ImGui::SetCurrentContext(context_);

    ImGuiIO& io = ImGui::GetIO();
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

    [[maybe_unused]] bool const init_sdl{
        ImGui_ImplSDL3_InitForVulkan(window.native_handle())};
    assert(init_sdl);

    [[maybe_unused]] bool const load_functions{
        ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3,
            load_vulkan_function,
            instance.handle)};
    assert(load_functions);

    VkPipelineRenderingCreateInfoKHR rendering_create_info{};
    rendering_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_create_info.colorAttachmentCount = 1;
    VkFormat const image_format{swapchain.image_format()};
    rendering_create_info.pColorAttachmentFormats = &image_format;

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = instance.handle;
    init_info.PhysicalDevice = device;
    init_info.Device = device;
    init_info.QueueFamily = present_queue.queue_family();
    init_info.Queue = present_queue.queue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool_;
    init_info.RenderPass = VK_NULL_HANDLE;
    init_info.Subpass = 0;
    init_info.MinImageCount = swapchain.min_image_count();
    init_info.ImageCount = swapchain.image_count();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.CheckVkResultFn = imgui_vulkan_result_callback;
    init_info.UseDynamicRendering = true;
    init_info.PipelineRenderingCreateInfo = rendering_create_info;
    [[maybe_unused]] bool const init_vulkan{ImGui_ImplVulkan_Init(&init_info)};
    assert(init_vulkan);
}

ngnwsi::imgui_layer_t::~imgui_layer_t()
{
    ImGui::SetCurrentContext(context_);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();

    ImGui::DestroyContext(context_);

    vkDestroyDescriptorPool(*device_, descriptor_pool_, nullptr);
}

bool ngnwsi::imgui_layer_t::handle_event(SDL_Event const& event) const
{
    if (enabled_)
    {
        ImGui::SetCurrentContext(context_);
        return ImGui_ImplSDL3_ProcessEvent(&event);
    }

    return false;
}

void ngnwsi::imgui_layer_t::begin_frame()
{
    ImGui::SetCurrentContext(context_);
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    frame_rendered_ = false;
}

void ngnwsi::imgui_layer_t::render(VkCommandBuffer command_buffer,
    vkrndr::image_t const& target_image)
{
    if (enabled_)
    {
        ImGui::SetCurrentContext(context_);
        ImGui::Render();
        frame_rendered_ = true;

        vkrndr::render_pass_t render_pass;
        render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            target_image.view);
        {
            VKRNDR_IF_DEBUG_UTILS(
                [[maybe_unused]] vkrndr::command_buffer_scope_t const cb_scope{
                    command_buffer,
                    "ImGUI"});
            [[maybe_unused]] auto const guard{render_pass.begin(command_buffer,
                {{0, 0}, vkrndr::to_2d_extent(target_image.extent)})};
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                command_buffer);
        }
    }
}

void ngnwsi::imgui_layer_t::end_frame()
{
    if (!std::exchange(frame_rendered_, true))
    {
        ImGui::SetCurrentContext(context_);
        ImGui::EndFrame();
    }
}
