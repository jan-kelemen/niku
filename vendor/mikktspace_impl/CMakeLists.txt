add_library(mikktspace_impl)

target_sources(mikktspace_impl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/mikktspace.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mikktspace.c
)

target_include_directories(mikktspace_impl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mikktspace.c 
    PROPERTIES
        SKIP_LINTING ON
)

set_target_properties(mikktspace_impl PROPERTIES FOLDER "vendor")

