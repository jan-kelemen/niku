add_executable(galileo)

target_sources(galileo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/deferred_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/frame_info.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gbuffer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gbuffer_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/materials.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_debug.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_engine.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/postprocess_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/render_graph.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/deferred_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/frame_info.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/galileo.m.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gbuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gbuffer_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/materials.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_debug.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/postprocess_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/render_graph.cpp
)

target_include_directories(galileo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(galileo
    PRIVATE
        cppext
        ngngfx
        ngnphy
        ngnwsi
        vkglsl
        vkgltf
    PRIVATE
        Angelscript::angelscript
        Angelscript::scriptbuilder
        Angelscript::scriptstdstring
        Boost::headers
        fmt::fmt
        imgui::imgui
        Jolt::Jolt
        spdlog::spdlog
    PRIVATE
        project-options
)

add_dependencies(galileo galileo_assets galileo_scripts galileo_shaders)

add_custom_target(galileo_assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/world.glb
        ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/world.glb
)

add_custom_target(galileo_scripts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test.as
        ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test.as
)

add_custom_target(galileo_shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${NIKU_DEMO_SHARED_DIR}/shaders/color_conversion.glsl
        ${NIKU_DEMO_SHARED_DIR}/shaders/Fxaa3_11.h
        ${NIKU_DEMO_SHARED_DIR}/shaders/pbrNeutral.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/frame_info.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fxaa.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gbuffer.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gbuffer.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/materials.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/math_constants.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/physics_debug_line.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/physics_debug_line.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/render_graph.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tone_mapping.comp
        ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${NIKU_DEMO_SHARED_DIR}/shaders/color_conversion.glsl
        ${NIKU_DEMO_SHARED_DIR}/shaders/Fxaa3_11.h
        ${NIKU_DEMO_SHARED_DIR}/shaders/pbrNeutral.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/frame_info.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fxaa.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gbuffer.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gbuffer.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/materials.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/math_constants.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/physics_debug_line.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/physics_debug_line.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/render_graph.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tone_mapping.comp
)

set_property(TARGET galileo 
    PROPERTY 
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(galileo PROPERTIES FOLDER "demo/galileo")
set_target_properties(galileo_assets PROPERTIES FOLDER "demo/galileo")
set_target_properties(galileo_scripts PROPERTIES FOLDER "demo/galileo")
set_target_properties(galileo_shaders PROPERTIES FOLDER "demo/galileo")
