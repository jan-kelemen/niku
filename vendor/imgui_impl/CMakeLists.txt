add_library(imgui_vulkan_impl)

target_sources(imgui_vulkan_impl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui_impl_vulkan.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_impl_vulkan.cpp
)

target_include_directories(imgui_vulkan_impl SYSTEM
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(imgui_vulkan_impl
    PUBLIC
        imgui::imgui
        vulkan-headers::vulkan-headers
)

add_library(imgui_sdl_impl)

target_sources(imgui_sdl_impl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui_impl_sdl2.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_impl_sdl2.cpp
)

target_include_directories(imgui_sdl_impl SYSTEM
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(imgui_sdl_impl
    PUBLIC
        imgui::imgui
        SDL2::SDL2
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_impl_vulkan.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_impl_sdl2.cpp 
    PROPERTIES
        SKIP_LINTING ON
)

set_target_properties(imgui_vulkan_impl PROPERTIES FOLDER "vendor")
set_target_properties(imgui_sdl_impl PROPERTIES FOLDER "vendor")
