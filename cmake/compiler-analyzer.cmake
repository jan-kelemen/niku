add_library(compiler-analyzer INTERFACE)

if(MSVC)
    target_compile_options(compiler-analyzer
        INTERFACE
            $<$<COMPILE_LANGUAGE:C,CXX>:/analyze /analyze:external->)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(compiler-analyzer
        INTERFACE
            $<$<COMPILE_LANGUAGE:C>:-fanalyze>)
else()
    message(AUTHOR_WARNING "No compiler analyzer options for CXX compiler: '${CMAKE_CXX_COMPILER_ID}'")
endif()

