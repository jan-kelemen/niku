add_executable(galileo)

target_sources(galileo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/batch_renderer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/character.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/character_contact_listener.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/deferred_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/follow_camera_controller.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/frame_info.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gbuffer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gbuffer_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/materials.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/navmesh.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/navmesh_debug.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_debug.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_engine.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/postprocess_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene_graph.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scripting.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sphere.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/world.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/world_contact_listener.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/batch_renderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/character.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/character_contact_listener.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/deferred_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/follow_camera_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/frame_info.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/galileo.m.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gbuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gbuffer_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/materials.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/navmesh.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/navmesh_debug.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_debug.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/postprocess_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene_graph.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scripting.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sphere.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/world.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/world_contact_listener.cpp
)

target_compile_definitions(galileo 
    PRIVATE
        GALILEO_SHADER_DEBUG_SYMBOLS=$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>
        GALILEO_SHADER_OPTIMIZATION=$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>
        GALILEO_VALIDATION_LAYERS=$<CONFIG:Debug>
)

target_include_directories(galileo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(galileo
    PRIVATE
        cppext
        glm_impl
        ngnast
        ngngfx
        ngnphy
        ngnscr
        ngnwsi
        vkglsl
    PRIVATE
        Boost::headers
        EnTT::EnTT
        fmt::fmt
        imgui::imgui
        Jolt::Jolt
        recastnavigation::recastnavigation
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
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/spawner.as
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/sphere.as
        ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/sphere.as
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/spawner.as
)

add_custom_target(galileo_shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${NIKU_DEMO_SHARED_DIR}/shaders/color_conversion.glsl
        ${NIKU_DEMO_SHARED_DIR}/shaders/Fxaa3_11.h
        ${NIKU_DEMO_SHARED_DIR}/shaders/math_constants.glsl
        ${NIKU_DEMO_SHARED_DIR}/shaders/pbrNeutral.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/frame_info.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fxaa.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gbuffer.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gbuffer.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/materials.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/debug.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/debug.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_graph.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tone_mapping.comp
        ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${NIKU_DEMO_SHARED_DIR}/shaders/color_conversion.glsl
        ${NIKU_DEMO_SHARED_DIR}/shaders/Fxaa3_11.h
        ${NIKU_DEMO_SHARED_DIR}/shaders/math_constants.glsl
        ${NIKU_DEMO_SHARED_DIR}/shaders/pbrNeutral.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deferred.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/frame_info.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fxaa.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gbuffer.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gbuffer.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/materials.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/debug.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/debug.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_graph.glsl
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
