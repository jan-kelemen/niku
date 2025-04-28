add_executable(reshed)

target_sources(reshed
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/reshed.m.cpp
)

target_compile_definitions(reshed
    PRIVATE
        RESHED_SHADER_DEBUG_SYMBOLS=$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>
        RESHED_SHADER_OPTIMIZATION=$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>
        RESHED_VALIDATION_LAYERS=$<CONFIG:Debug>
)

target_include_directories(reshed
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(reshed
    PRIVATE
        ngnwsi
        vkrndr
    PRIVATE
        imgui::imgui
    PRIVATE
        project-options
)

set_property(TARGET reshed
    PROPERTY
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(reshed PROPERTIES FOLDER "demo/reshed")
