add_library(ngnwsi)

target_sources(ngnwsi
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnwsi_application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnwsi_imgui_layer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnwsi_mouse.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnwsi_render_window.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnwsi_sdl_window.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnwsi_application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnwsi_imgui_layer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnwsi_mouse.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnwsi_render_window.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnwsi_sdl_window.cpp
)

target_include_directories(ngnwsi
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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
        $<BUILD_INTERFACE:project-options>
)

install(TARGETS ngnwsi
    EXPORT ${PROJECT_NAME}-config
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_runtime
        NAMELINK_COMPONENT ${PROJECT_NAME}_dev
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_dev
)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

add_library(niku::ngnwsi ALIAS ngnwsi)
