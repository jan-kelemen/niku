add_library(ngnwsi)

target_sources(ngnwsi
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnwsi_application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnwsi_imgui_layer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnwsi_mouse.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnwsi_sdl_window.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnwsi_application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnwsi_imgui_layer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnwsi_mouse.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnwsi_sdl_window.cpp
)

target_include_directories(ngnwsi
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ngnwsi
    PUBLIC
        vkrndr
        glm_impl
    PUBLIC
        SDL3::SDL3
    PRIVATE
        imgui::imgui
        spdlog::spdlog
    PRIVATE
        imgui_sdl_impl
        imgui_vulkan_impl
    PRIVATE
        project-options
)

