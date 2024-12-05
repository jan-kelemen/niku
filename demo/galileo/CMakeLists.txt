add_executable(galileo)

target_sources(galileo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_debug.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_engine.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/camera_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/galileo.m.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_debug.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_engine.cpp
)

target_include_directories(galileo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(galileo
    PRIVATE
        cppext
        niku
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

add_dependencies(galileo galileo_scripts galileo_shaders)

add_custom_target(galileo_scripts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test.as
        ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test.as
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/physics_debug_line.frag
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/physics_debug_line.frag.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/physics_debug_line.vert
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/physics_debug_line.vert.spv
)

add_custom_target(galileo_shaders
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/physics_debug_line.frag.spv
        ${CMAKE_CURRENT_BINARY_DIR}/physics_debug_line.vert.spv
)

set_property(TARGET galileo 
    PROPERTY 
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(galileo PROPERTIES FOLDER "demo/galileo")
set_target_properties(galileo_scripts PROPERTIES FOLDER "demo/galileo")
set_target_properties(galileo_shaders PROPERTIES FOLDER "demo/galileo")
