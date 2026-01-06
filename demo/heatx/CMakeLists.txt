add_executable(heatx)

target_sources(heatx
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/materials.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/postprocess_shader.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/heatx.m.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/materials.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/postprocess_shader.cpp
)

target_compile_definitions(heatx
    PRIVATE
        HEATX_SHADER_DEBUG_SYMBOLS=$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>
        HEATX_SHADER_OPTIMIZATION=$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>
        HEATX_VALIDATION_LAYERS=$<CONFIG:Debug>
)

target_include_directories(heatx
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(heatx
    PRIVATE
        ngnast
        ngngfx
        ngnwsi
        vkrndr
        vkglsl
    PRIVATE
        imgui::imgui
        spdlog::spdlog
    PRIVATE
        project-options
)

add_dependencies(heatx heatx_shaders)

add_copy_if_different_target(heatx_shaders
    DESTINATION
        ${CMAKE_CURRENT_BINARY_DIR}
    FILES
        ${NIKU_DEMO_SHARED_DIR}/shaders/color_conversion.glsl
        ${NIKU_DEMO_SHARED_DIR}/shaders/Fxaa3_11.h
        ${NIKU_DEMO_SHARED_DIR}/shaders/math_constants.glsl
        ${NIKU_DEMO_SHARED_DIR}/shaders/pbrNeutral.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/closesthit.rchit
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fxaa.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/miss.rmiss
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/raygen.rgen
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/miss.rmiss
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tone_mapping.comp
)

set_property(TARGET heatx
    PROPERTY
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(heatx PROPERTIES FOLDER "demo/heatx")
set_target_properties(heatx_shaders PROPERTIES FOLDER "demo/heatx")

