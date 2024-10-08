add_library(vkrndr)

target_sources(vkrndr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_backend.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_buffer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_command_pool.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_commands.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_context.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_depth_buffer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_descriptors.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_device.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_execution_port.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_image.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_memory.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_pipeline.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_render_pass.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_render_settings.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_shader_module.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_swap_chain.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_synchronization.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_utility.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_window.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_backend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_buffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_command_pool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_commands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_depth_buffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_descriptors.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_device.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_execution_port.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_global_data.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_global_data.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_image.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_render_pass.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_shader_module.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_swap_chain.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_synchronization.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_utility.cpp
)

target_include_directories(vkrndr
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(vkrndr
    PUBLIC
        cppext
        vma_impl
    PUBLIC
        volk::volk
    PRIVATE
        fmt::fmt
        spdlog::spdlog
    PRIVATE
        project-options
)

target_compile_definitions(vkrndr
    PUBLIC
        VK_NO_PROTOTYPES
)
