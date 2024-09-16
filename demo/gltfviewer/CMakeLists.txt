add_executable(gltfviewer)

target_sources(gltfviewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model_selector.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pbr_renderer.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gltfviewer.m.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model_selector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pbr_renderer.cpp
)

target_include_directories(gltfviewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(gltfviewer
    PRIVATE
        niku
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

add_dependencies(gltfviewer shaders)

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

add_custom_target(shaders
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/pbr.frag.spv
        ${CMAKE_CURRENT_BINARY_DIR}/pbr.vert.spv
)

set_property(TARGET gltfviewer 
    PROPERTY 
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(gltfviewer PROPERTIES FOLDER "demo")
