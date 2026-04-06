add_executable(editor)

target_sources(editor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/editor_application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/grid_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/material_manager.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/editor_application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/editor.m.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/grid_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/material_manager.cpp
)

target_include_directories(editor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(editor
    PRIVATE
        cppext
        ngnast
        ngngfx
        ngnwsi
        tracy_impl
        vkglsl
        vkrndr
    PRIVATE
        Boost::headers
        EnTT::EnTT
        spdlog::spdlog
        imgui::imgui
    PRIVATE
        $<BUILD_INTERFACE:project-options>
)

target_compile_definitions(editor 
    PRIVATE
        EDITOR_SHADER_DEBUG_SYMBOLS=$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>
        EDITOR_SHADER_OPTIMIZATION=$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>
)

add_dependencies(editor editor_shaders)

add_copy_if_different_target(editor_shaders
    DESTINATION
        ${CMAKE_CURRENT_BINARY_DIR}
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/grid.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/grid.frag
)

set_property(TARGET editor
    PROPERTY
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(editor PROPERTIES FOLDER "editor")
set_target_properties(editor_shaders PROPERTIES FOLDER "editor")
