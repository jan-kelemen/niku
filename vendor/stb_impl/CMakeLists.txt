add_library(stb_impl)

target_sources(stb_impl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stb_image_impl.cpp
)

target_link_libraries(stb_impl
    PUBLIC
        stb::stb
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stb_image_impl.cpp 
    PROPERTIES
        SKIP_LINTING ON
)

set_target_properties(stb_impl PROPERTIES FOLDER "vendor")