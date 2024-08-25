if (NIKU_ENABLE_IWYU)
    find_program(IWYU_EXE NAMES include-what-you-use iwyu REQUIRED)
    message(STATUS "include-what-you-use found: ${IWYU_EXE}")

    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE 
        ${IWYU_EXE};
        -Wno-unknown-warning-option;
        -Wno-unknown-attributes;
        -Xiwyu;--comment_style=long;
        -Xiwyu;--cxx17ns;
        -Xiwyu;--mapping_file=${PROJECT_SOURCE_DIR}/iwyu.imp
    )
endif()

