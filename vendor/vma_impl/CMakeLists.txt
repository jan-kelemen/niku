add_library(vma_impl)

target_sources(vma_impl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vma_impl.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vma_impl.cpp
)

target_include_directories(vma_impl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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

install(TARGETS vma_impl
    EXPORT ${PROJECT_NAME}-config
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_runtime
        NAMELINK_COMPONENT ${PROJECT_NAME}_dev
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT ${PROJECT_NAME}_dev
)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

add_library(niku::vma_impl ALIAS vma_impl)

set_target_properties(vma_impl PROPERTIES FOLDER "vendor")
