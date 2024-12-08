#include <ngnwsi_sdl_window.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

ngnwsi::sdl_guard_t::sdl_guard_t(uint32_t const flags)
{
    if (SDL_Init(flags) != 0)
    {
        throw std::runtime_error{SDL_GetError()};
    }
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
}

ngnwsi::sdl_guard_t::~sdl_guard_t() { SDL_Quit(); }

ngnwsi::sdl_window_t::sdl_window_t(char const* const title,
    SDL_WindowFlags const window_flags,
    bool const centered,
    int const width,
    int const height)
    : window_{SDL_CreateWindow(title,
          centered ? SDL_WINDOWPOS_CENTERED : SDL_WINDOWPOS_UNDEFINED,
          centered ? SDL_WINDOWPOS_CENTERED : SDL_WINDOWPOS_UNDEFINED,
          width,
          height,
          static_cast<SDL_WindowFlags>(window_flags | SDL_WINDOW_VULKAN))}
{
    if (window_ == nullptr)
    {
        throw std::runtime_error{SDL_GetError()};
    }
}

ngnwsi::sdl_window_t::~sdl_window_t() { SDL_DestroyWindow(window_); }

std::vector<char const*> ngnwsi::sdl_window_t::required_extensions() const
{
    unsigned int extension_count{};
    SDL_Vulkan_GetInstanceExtensions(window_, &extension_count, nullptr);
    std::vector<char const*> required_extensions(extension_count, nullptr);
    SDL_Vulkan_GetInstanceExtensions(window_,
        &extension_count,
        required_extensions.data());

    return required_extensions;
}

VkResult ngnwsi::sdl_window_t::create_surface(VkInstance instance,
    VkSurfaceKHR& surface) const
{
    if (SDL_Vulkan_CreateSurface(window_, instance, &surface) == SDL_TRUE)
    {
        return VK_SUCCESS;
    }
    return VK_ERROR_UNKNOWN;
}

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

bool ngnwsi::sdl_window_t::is_minimized() const
{
    return (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) != 0;
}
