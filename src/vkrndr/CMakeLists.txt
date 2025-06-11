add_library(vkrndr)

target_sources(vkrndr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_backend.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_buffer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_command_pool.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_commands.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_context.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_cubemap.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_debug_utils.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_descriptors.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_device.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_execution_port.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_features.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_formats.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_image.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_library.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_memory.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_pipeline.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_render_pass.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_render_settings.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_shader_module.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_swap_chain.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_synchronization.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_transient_operation.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_utility.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkrndr_window.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_backend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_buffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_command_pool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_commands.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_cubemap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_debug_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_descriptors.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_device.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_execution_port.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_features.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_formats.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_global_data.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_global_data.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_image.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_library.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_render_pass.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_shader_module.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_swap_chain.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_synchronization.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_transient_operation.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkrndr_utility.cpp
)

target_include_directories(vkrndr
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(vkrndr
    PUBLIC
        cppext
        vma_impl
    PUBLIC
        volk::volk
        vulkan-utility-libraries::vulkan-utility-libraries
    PRIVATE
        Boost::headers
        fmt::fmt
        spdlog::spdlog
    PRIVATE
        $<BUILD_INTERFACE:project-options>
)

target_compile_definitions(vkrndr
    PUBLIC
        VK_NO_PROTOTYPES
)

install(TARGETS vkrndr
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

add_library(niku::vkrndr ALIAS vkrndr)

if (NOT DEFINED NIKU_VKRNDR_ENABLE_DEBUG_UTILS)
    set(VKRNDR_ENABLE_DEBUG_UTILS $<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>)
endif()

if (VKRNDR_ENABLE_DEBUG_UTILS)
    target_compile_definitions(vkrndr
        PUBLIC 
            VKRNDR_ENABLE_DEBUG_UTILS=1
    )
endif()
