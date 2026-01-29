add_executable(editor)

target_sources(editor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/editor_application.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/editor_application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/editor.m.cpp
)

target_include_directories(editor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(editor
    PRIVATE
        cppext
        ngnwsi
        vkrndr
    PRIVATE
        spdlog::spdlog
    PRIVATE
        project-options
)

set_property(TARGET editor
    PROPERTY
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

