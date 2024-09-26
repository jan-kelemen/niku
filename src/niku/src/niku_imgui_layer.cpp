#include <niku_imgui_layer.hpp>

#include <niku_sdl_window.hpp>

#include <vkrndr_context.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_swap_chain.hpp>
#include <vkrndr_utility.hpp>

#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <imgui.h>

#include <spdlog/spdlog.h>

#include <cassert>
#include <utility>

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
            vkCreateDescriptorPool(device.logical, &pool_info, nullptr, &rv));

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

niku::imgui_layer_t::imgui_layer_t(sdl_window_t const& window,
    vkrndr::context_t const& context,
    vkrndr::device_t& device,
    vkrndr::swap_chain_t const& swap_chain)
    : device_{&device}
    , descriptor_pool_{create_descriptor_pool(device)}
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
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
        ImGui_ImplSDL2_InitForVulkan(window.native_handle())};
    assert(init_sdl);

    [[maybe_unused]] bool const load_functions{
        ImGui_ImplVulkan_LoadFunctions(load_vulkan_function, context.instance)};
    assert(load_functions);

    VkPipelineRenderingCreateInfoKHR rendering_create_info{};
    rendering_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_create_info.colorAttachmentCount = 1;
    VkFormat const image_format{swap_chain.image_format()};
    rendering_create_info.pColorAttachmentFormats = &image_format;

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = context.instance;
    init_info.PhysicalDevice = device.physical;
    init_info.Device = device.logical;
    init_info.QueueFamily = device.present_queue->queue_family();
    init_info.Queue = device.present_queue->queue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool_;
    init_info.RenderPass = VK_NULL_HANDLE;
    init_info.Subpass = 0;
    init_info.MinImageCount = swap_chain.min_image_count();
    init_info.ImageCount = swap_chain.image_count();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.CheckVkResultFn = imgui_vulkan_result_callback;
    init_info.UseDynamicRendering = true;
    init_info.PipelineRenderingCreateInfo = rendering_create_info;
    [[maybe_unused]] bool const init_vulkan{ImGui_ImplVulkan_Init(&init_info)};
    assert(init_vulkan);
}

niku::imgui_layer_t::~imgui_layer_t()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(device_->logical, descriptor_pool_, nullptr);
}

bool niku::imgui_layer_t::handle_event(SDL_Event const& event) const
{
    if (enabled_)
    {
        return ImGui_ImplSDL2_ProcessEvent(&event);
    }

    return false;
}

void niku::imgui_layer_t::begin_frame()
{
    if (enabled_)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        frame_rendered_ = false;
    }
}

void niku::imgui_layer_t::render(VkCommandBuffer command_buffer,
    vkrndr::image_t const& target_image)
{
    if (enabled_)
    {
        ImGui::Render();
        frame_rendered_ = true;

        vkrndr::render_pass_t render_pass;
        render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            target_image.view);
        {
            [[maybe_unused]] auto const guard{render_pass.begin(command_buffer,
                {{0, 0}, target_image.extent})};
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                command_buffer);
        }
    }
}

void niku::imgui_layer_t::end_frame()
{
    if (enabled_ && !std::exchange(frame_rendered_, true))
    {
        spdlog::info("Ending ImGui frame without matching draw call");
        ImGui::EndFrame();
    }
}
