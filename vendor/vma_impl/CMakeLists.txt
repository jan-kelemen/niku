add_library(vma_impl)

target_sources(vma_impl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vma_impl.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vma_impl.cpp
)

target_include_directories(vma_impl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vma_impl
    PUBLIC
        vulkan-memory-allocator::vulkan-memory-allocator
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vma_impl.cpp 
    PROPERTIES
        SKIP_LINTING ON
)

target_compile_definitions(vma_impl
    PUBLIC
        VK_NO_PROTOTYPES
)

set_target_properties(vma_impl PROPERTIES FOLDER "vendor")