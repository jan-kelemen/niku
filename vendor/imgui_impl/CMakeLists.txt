add_library(imgui_vulkan_impl)

target_sources(imgui_vulkan_impl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui_impl_vulkan.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_impl_vulkan.cpp
)

target_include_directories(imgui_vulkan_impl SYSTEM
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(imgui_vulkan_impl
    PUBLIC
        imgui::imgui
        volk::volk
)

target_compile_definitions(imgui_vulkan_impl
    PUBLIC
        IMGUI_IMPL_VULKAN_USE_VOLK
        VK_NO_PROTOTYPES
)

install(TARGETS imgui_vulkan_impl
    EXPORT ${PROJECT_NAME}-config
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_runtime
        NAMELINK_COMPONENT ${PROJECT_NAME}_dev
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_dev
)

add_library(niku::imgui_vulkan_impl ALIAS imgui_vulkan_impl)

add_library(imgui_sdl_impl)

target_sources(imgui_sdl_impl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui_impl_sdl3.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_impl_sdl3.cpp
)

target_include_directories(imgui_sdl_impl SYSTEM
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(imgui_sdl_impl
    PUBLIC
        imgui::imgui
        SDL3::SDL3
)

install(TARGETS imgui_sdl_impl
    EXPORT ${PROJECT_NAME}-config
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_runtime
        NAMELINK_COMPONENT ${PROJECT_NAME}_dev
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_dev
)

add_library(niku::imgui_sdl_impl ALIAS imgui_sdl_impl)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_impl_vulkan.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_impl_sdl3.cpp
    PROPERTIES
        SKIP_LINTING ON
)

set_target_properties(imgui_vulkan_impl PROPERTIES FOLDER "vendor")
set_target_properties(imgui_sdl_impl PROPERTIES FOLDER "vendor")
