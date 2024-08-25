if (NIKU_ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXE NAMES cppcheck REQUIRED)
    message(STATUS "cppcheck found: ${CPPCHECK_EXE}")

    if(MSVC)
        set(CPPCHECK_TEMPLATE "vs")
    else()
        set(CPPCHECK_TEMPLATE "gcc")
    endif()

    set(CMAKE_CXX_CPPCHECK
        ${CPPCHECK_EXE}
        --template=${CPPCHECK_TEMPLATE}
        --enable=style,performance,warning,portability
        --inline-suppr
        # We cannot act on a bug/missing feature of cppcheck
        --suppress=cppcheckError
        --suppress=internalAstError
        # if a file does not have an internalAstError, we get an unmatchedSuppression error
        --suppress=unmatchedSuppression
        # noisy and incorrect sometimes
        --suppress=passedByValue
        --suppress=knownConditionTrueFalse
        --suppress=functionStatic
        # ignores code that cppcheck thinks is invalid C++
        --suppress=syntaxError
        --suppress=preprocessorErrorDirective
        --inconclusive)

    if(NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
        set(CMAKE_CXX_CPPCHECK ${CMAKE_CXX_CPPCHECK} --std=c++${CMAKE_CXX_STANDARD})
    endif()
endif()

