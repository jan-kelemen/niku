add_executable(gltfviewer)

target_sources(gltfviewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/depth_pass_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/environment.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/materials.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model_selector.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pbr_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/postprocess_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pyramid_blur.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene_graph.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/resolve_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/skybox.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/weighted_blend_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/weighted_oit_shader.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/depth_pass_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/environment.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gltfviewer.m.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/materials.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model_selector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pbr_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/postprocess_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pyramid_blur.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene_graph.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/resolve_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/skybox.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/weighted_blend_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/weighted_oit_shader.cpp
)

target_compile_definitions(gltfviewer 
    PRIVATE
        GLTFVIEWER_SHADER_DEBUG_SYMBOLS=$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>
        GLTFVIEWER_SHADER_OPTIMIZATION=$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>
        GLTFVIEWER_VALIDATION_LAYERS=$<CONFIG:Debug>
)

target_include_directories(gltfviewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(gltfviewer
    PRIVATE
        ngnast
        ngngfx
        ngnwsi
        glm_impl
        stb_impl
        vkglsl
        vkrndr
    PRIVATE
        Boost::headers
        fmt::fmt
        imgui::imgui
        simdjson::simdjson
        spdlog::spdlog
    PRIVATE
        project-options
)

add_dependencies(gltfviewer gltfviewer_assets gltfviewer_shaders)

add_custom_target(gltfviewer_assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/aviation_museum_4k.hdr
        ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/aviation_museum_4k.hdr
)

add_custom_target(gltfviewer_shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${NIKU_DEMO_SHARED_DIR}/shaders/color_conversion.glsl
        ${NIKU_DEMO_SHARED_DIR}/shaders/math_constants.glsl
        ${NIKU_DEMO_SHARED_DIR}/shaders/pbrNeutral.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/brdf.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/cubemap.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/environment.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/equirectangular_to_cubemap.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/irradiance.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/oit_composition.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/postprocess.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/prefilter.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pyramid_downsample.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pyramid_upsample.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/resolve.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/skybox.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/skybox.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/weighted_blend.comp
        ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${NIKU_DEMO_SHARED_DIR}/shaders/color_conversion.glsl
        ${NIKU_DEMO_SHARED_DIR}/shaders/math_constants.glsl
        ${NIKU_DEMO_SHARED_DIR}/shaders/pbrNeutral.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/brdf.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/cubemap.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/environment.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/equirectangular_to_cubemap.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/irradiance.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/oit_composition.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/postprocess.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/prefilter.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pyramid_downsample.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pyramid_upsample.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/resolve.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/skybox.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/skybox.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/weighted_blend.comp
)

set_property(TARGET gltfviewer 
    PROPERTY 
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(gltfviewer PROPERTIES FOLDER "demo/gltfviewer")
set_target_properties(gltfviewer_shaders PROPERTIES FOLDER "demo/gltfviewer")
set_target_properties(gltfviewer_assets PROPERTIES FOLDER "demo/gltfviewer")
