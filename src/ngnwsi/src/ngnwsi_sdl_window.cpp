#include <ngnwsi_sdl_window.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

ngnwsi::sdl_guard_t::sdl_guard_t(uint32_t const flags)
{
    if (!SDL_Init(flags))
    {
        throw std::runtime_error{SDL_GetError()};
    }
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "1");
}

ngnwsi::sdl_guard_t::~sdl_guard_t() { SDL_Quit(); }

std::vector<char const*> ngnwsi::sdl_window_t::required_extensions()
{
    unsigned int extension_count{};
    auto const* const extensions{
        SDL_Vulkan_GetInstanceExtensions(&extension_count)};

    return {extensions, extensions + extension_count};
}

ngnwsi::sdl_window_t::sdl_window_t(char const* const title,
    SDL_WindowFlags const window_flags,
    int const width,
    int const height)
    : window_{SDL_CreateWindow(title,
          width,
          height,
          window_flags | SDL_WINDOW_VULKAN)}
{
    if (window_ == nullptr)
    {
        throw std::runtime_error{SDL_GetError()};
    }
}

ngnwsi::sdl_window_t::~sdl_window_t() { SDL_DestroyWindow(window_); }

bool ngnwsi::sdl_window_t::is_minimized() const
{
    return (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) != 0;
}

VkSurfaceKHR ngnwsi::sdl_window_t::create_surface(VkInstance instance)
{
    if (SDL_Vulkan_CreateSurface(window_, instance, nullptr, &surface_))
    {
        return surface_;
    }

    return surface_;
}

void ngnwsi::sdl_window_t::destroy_surface(VkInstance instance)
{
    vkDestroySurfaceKHR(instance, surface_, nullptr);
}

VkSurfaceKHR ngnwsi::sdl_window_t::surface() const { return surface_; }

VkExtent2D ngnwsi::sdl_window_t::swap_extent(
    VkSurfaceCapabilitiesKHR const& capabilities) const
{
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    int width{};
    int height{};
    SDL_GetWindowSize(window_, &width, &height);

    VkExtent2D actual_extent = {static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)};

    actual_extent.width = std::clamp(actual_extent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    actual_extent.height = std::clamp(actual_extent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);

    return actual_extent;
}

ngnwsi::sdl_text_input_guard_t::sdl_text_input_guard_t(
    sdl_window_t const& window)
    : window_{&window}
{
    if (!SDL_StartTextInput(window_->native_handle()))
    {
        throw std::runtime_error{SDL_GetError()};
    }
}

ngnwsi::sdl_text_input_guard_t::~sdl_text_input_guard_t()
{
    SDL_StopTextInput(window_->native_handle());
}
