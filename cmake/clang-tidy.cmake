if (NIKU_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy-18 clang-tidy REQUIRED)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    
    set(CMAKE_CXX_CLANG_TIDY
        ${CLANG_TIDY_EXE};
        -format-style='file';
        -header-filter=${PROJECT_SOURCE_DIR}/src;
        -extra-arg=-Wno-unknown-warning-option
    )
endif()

