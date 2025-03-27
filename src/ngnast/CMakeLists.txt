add_library(ngnast)

target_sources(ngnast
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnast_error.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnast_gltf_loader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnast_gpu_transfer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnast_mesh_transform.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ngnast_scene_model.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnast_error.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnast_gltf_fastgltf_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnast_gltf_fastgltf_adapter.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnast_gltf_loader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnast_gpu_transfer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnast_mesh_transform.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ngnast_scene_model.cpp
)

target_include_directories(ngnast
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ngnast
    PUBLIC
        vkrndr
        glm_impl
    PRIVATE
        cppext
        stb_impl
    PRIVATE
        Boost::headers
        fastgltf::fastgltf
        fmt::fmt
        mikktspace::mikktspace 
        meshoptimizer::meshoptimizer
        spdlog::spdlog
    PRIVATE
        $<BUILD_INTERFACE:project-options>
)

install(TARGETS ngnast
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

add_library(niku::ngnast ALIAS ngnast)
