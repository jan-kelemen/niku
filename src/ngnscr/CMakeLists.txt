add_library(ngnscr)

target_sources(ngnscr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnscr_scripting_engine.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnscr_scripting_engine.cpp
)

target_include_directories(ngnscr
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ngnscr
    PUBLIC
        Angelscript::angelscript
        Angelscript::scriptbuilder
    PRIVATE
        Angelscript::scriptstdstring
        spdlog::spdlog
    PRIVATE
        project-options
)

