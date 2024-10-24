add_executable(gltfviewer)

target_sources(gltfviewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/environment.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/materials.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model_selector.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pbr_renderer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/postprocess_shader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/render_graph.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/skybox.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/environment.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gltfviewer.m.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/materials.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model_selector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pbr_renderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/postprocess_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/render_graph.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/skybox.cpp
)

target_include_directories(gltfviewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(gltfviewer
    PRIVATE
        niku
        stb_impl
        vkgltf
        vkrndr
    PRIVATE
        fmt::fmt
        glm::glm
        imgui::imgui
        simdjson::simdjson
        spdlog::spdlog
    PRIVATE
        project-options
)

add_dependencies(gltfviewer gltfviewer_assets gltfviewer_shaders)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/brdf.frag
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/brdf.frag.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/cubemap.vert
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/cubemap.vert.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/equirectangular_to_cubemap.frag
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/equirectangular_to_cubemap.frag.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/fullscreen.vert.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ibl_debug.frag
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/ibl_debug.frag.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/irradiance.frag
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/irradiance.frag.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.frag
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/pbr.frag.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.vert
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/pbr.vert.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/prefilter.frag
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/prefilter.frag.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/postprocess.frag
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/postprocess.frag.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/skybox.frag
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/skybox.frag.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/skybox.vert
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/skybox.vert.spv
)

add_custom_target(gltfviewer_shaders
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/brdf.frag.spv
        ${CMAKE_CURRENT_BINARY_DIR}/cubemap.vert.spv
        ${CMAKE_CURRENT_BINARY_DIR}/equirectangular_to_cubemap.frag.spv
        ${CMAKE_CURRENT_BINARY_DIR}/fullscreen.vert.spv
        ${CMAKE_CURRENT_BINARY_DIR}/ibl_debug.frag.spv
        ${CMAKE_CURRENT_BINARY_DIR}/irradiance.frag.spv
        ${CMAKE_CURRENT_BINARY_DIR}/pbr.frag.spv
        ${CMAKE_CURRENT_BINARY_DIR}/pbr.vert.spv
        ${CMAKE_CURRENT_BINARY_DIR}/prefilter.frag.spv
        ${CMAKE_CURRENT_BINARY_DIR}/postprocess.frag.spv
        ${CMAKE_CURRENT_BINARY_DIR}/skybox.frag.spv
        ${CMAKE_CURRENT_BINARY_DIR}/skybox.vert.spv
)

add_custom_target(gltfviewer_assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/aviation_museum_4k.hdr
        ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/aviation_museum_4k.hdr
)

set_property(TARGET gltfviewer 
    PROPERTY 
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(gltfviewer PROPERTIES FOLDER "demo")
set_target_properties(gltfviewer_shaders PROPERTIES FOLDER "demo")
set_target_properties(gltfviewer_assets PROPERTIES FOLDER "demo")
