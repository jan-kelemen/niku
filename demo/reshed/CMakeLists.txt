add_executable(reshed)

target_sources(reshed
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/editor_window.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/text_buffer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/text_editor.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/editor_window.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/reshed.m.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/text_buffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/text_editor.cpp
)

target_compile_definitions(reshed
    PRIVATE
        RESHED_SHADER_DEBUG_SYMBOLS=$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>
        RESHED_SHADER_OPTIMIZATION=$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>
)

target_include_directories(reshed
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(reshed
    PRIVATE
        ngngfx
        ngntxt
        ngnwsi
        vkrndr
        vkglsl
    PRIVATE
        imgui::imgui
        spdlog::spdlog
        tree-sitter-glsl::tree-sitter-glsl
    PRIVATE
        project-options
)

add_dependencies(reshed reshed_assets reshed_shaders)

add_copy_if_different_target(reshed_shaders
    DESTINATION
        ${CMAKE_CURRENT_BINARY_DIR}
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/text.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/text.tesc
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/text.tese
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/text.vert
)

add_copy_if_different_target(reshed_assets
    DESTINATION
        ${CMAKE_CURRENT_BINARY_DIR}
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/SpaceMono-Regular.ttf
        ${tree-sitter-glsl_INCLUDE_DIR}/../queries/highlights.scm
)

set_property(TARGET reshed
    PROPERTY
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(reshed PROPERTIES FOLDER "demo/reshed")
set_target_properties(reshed_assets PROPERTIES FOLDER "demo/reshed")
set_target_properties(reshed_shaders PROPERTIES FOLDER "demo/reshed")
