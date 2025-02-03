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
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ngnast
    PUBLIC
        vkrndr
        glm_impl
    PRIVATE
        cppext
        mikktspace_impl
        stb_impl
    PRIVATE
        Boost::headers
        fastgltf::fastgltf
        fmt::fmt
        meshoptimizer::meshoptimizer
        spdlog::spdlog
    PRIVATE
        project-options
)

