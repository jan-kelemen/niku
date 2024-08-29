add_library(niku)

target_sources(niku
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/niku_application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/niku_camera.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/niku_mouse.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/niku_perspective_camera.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/niku_application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/niku_application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/niku_camera.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/niku_mouse.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/niku_perspective_camera.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/niku_sdl_window.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/niku_sdl_window.cpp
)

target_include_directories(niku
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(niku
    PUBLIC
        vkrndr
    PUBLIC
        SDL2::SDL2
    PRIVATE
        project-options
)

