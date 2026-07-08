cmake_minimum_required(VERSION 3.10)

project(aparse LANGUAGES C)

option(APARSE_BUILD_ASAN    "Build aparse with AddressSanitizer" OFF)
option(APARSE_BUILD_EXAMPLE "Build aparse's examples & tests" OFF)

function(target_add_asan target_name)
    if(NOT APARSE_BUILD_ASAN)
        return()
    endif()

    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "AddressSanitizer is not supported by '${CMAKE_C_COMPILER_ID}'")
    endif()

    target_compile_options(${target_name} PRIVATE
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:
            -fsanitize=address
            -fno-omit-frame-pointer
        >
    )

    target_link_options(${target_name} PRIVATE
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:
            -fsanitize=address
        >
    )
endfunction()

function(target_add_strict_flags target_name)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        return()
    endif()

    target_compile_options(${target_name} PRIVATE
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:
            -Werror
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wcast-align
            -Wcast-qual
            -Wwrite-strings
            -Wstrict-prototypes
            -Wmissing-prototypes
            -Wshadow
            -Wswitch
            -Wundef
            -Wnull-dereference
            -Wformat=2
            -fdiagnostics-show-option
            -Winit-self
            -Wmissing-include-dirs
            -Wdisabled-optimization
            -Wno-unused-parameter
            -Wno-parentheses
            -Wno-variadic-macro-arguments-omitted
            -Wno-gnu-zero-variadic-macro-arguments
        >
    )
endfunction()

set(APARSE_SOURCES
    src/aparse.c
    src/aparse_list.c
)

add_library(aparse STATIC ${APARSE_SOURCES})

target_include_directories(aparse PUBLIC include
)

set_target_properties(aparse PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

target_add_asan(aparse)
target_add_strict_flags(aparse)

if(APARSE_BUILD_EXAMPLE)
    add_executable(example example/example.c)
    target_link_libraries(example PRIVATE aparse)
    target_add_asan(example)
    target_add_strict_flags(example)

    add_executable(subcmd example/subcmd_example.c)
    target_link_libraries(subcmd PRIVATE aparse)
    if(UNIX)
        target_link_libraries(subcmd PRIVATE m)
    endif()

    target_add_asan(subcmd)
    target_add_strict_flags(subcmd)
    add_executable(tests example/tests.c)
    target_link_libraries(tests PRIVATE aparse)
    target_add_asan(tests)
    target_add_strict_flags(tests)
endif()
