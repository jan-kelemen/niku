add_library(ngnphy)

target_sources(ngnphy
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnphy_jolt_adapter.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnphy_jolt_geometry.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnphy_jolt_adapter.cpp
)

target_include_directories(ngnphy
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ngnphy
    PUBLIC
        glm_impl
    PUBLIC
        Jolt::Jolt
    PRIVATE
        project-options
)

