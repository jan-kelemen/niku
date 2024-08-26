add_library(tinygltf_impl)

target_sources(tinygltf_impl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/tinygltf_impl.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tinygltf_impl.cpp
)

target_include_directories(tinygltf_impl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinygltf_impl
    PUBLIC
        TinyGLTF::TinyGLTF
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tinygltf_impl.cpp 
    PROPERTIES
        SKIP_LINTING ON
)

set_target_properties(tinygltf_impl PROPERTIES FOLDER "vendor")