add_library(vkgltf)

target_sources(vkgltf
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkgltf_error.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkgltf_loader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/vkgltf_model.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkgltf_error.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkgltf_fastgltf_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkgltf_fastgltf_adapter.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkgltf_loader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vkgltf_model.cpp
)

target_include_directories(vkgltf
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(vkgltf
    PUBLIC
        vkrndr
    PUBLIC
        glm::glm
        tl::expected
    PRIVATE
        cppext
        stb_impl
    PRIVATE
        Boost::headers
        fastgltf::fastgltf
        fmt::fmt
        meshoptimizer::meshoptimizer
        mikktspace::mikktspace 
        spdlog::spdlog
    PRIVATE
        project-options
)

