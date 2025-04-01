add_library(project-options INTERFACE)

include(${CMAKE_CURRENT_LIST_DIR}/compiler-warnings.cmake)

if(NIKU_ENABLE_CLANG_FORMAT)
    include(${CMAKE_CURRENT_LIST_DIR}/clang-format.cmake)
    add_dependencies(project-options clang-format)
endif()

if(NIKU_ENABLE_COMPILER_STATIC_ANALYSIS)
    include(${CMAKE_CURRENT_LIST_DIR}/compiler-analyzer.cmake)
    target_link_libraries(project-options
        INTERFACE 
            compiler-analyzer)
endif()

if (NIKU_ENABLE_COLORED_COMPILER_OUTPUT)
    if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set(COLORED_COMPILER_DIAGNOSTICS -fcolor-diagnostics)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(COLORED_COMPILER_DIAGNOSTICS -fdiagnostics-color=auto)
    endif()

    target_compile_options(project-options
        INTERFACE
            ${COLORED_COMPILER_DIAGNOSTICS})
endif()
