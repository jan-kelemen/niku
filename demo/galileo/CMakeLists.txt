add_executable(galileo)

target_sources(galileo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/galileo.m.cpp
)

target_include_directories(galileo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(galileo
    PRIVATE
        Angelscript::angelscript
        Angelscript::scriptbuilder
        Angelscript::scriptstdstring
        fmt::fmt
        spdlog::spdlog
    PRIVATE
        project-options
)

add_dependencies(galileo galileo_scripts)

add_custom_target(galileo_scripts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test.as
        ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test.as
)

set_property(TARGET galileo 
    PROPERTY 
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(galileo PROPERTIES FOLDER "demo")
