cmake_minimum_required(VERSION 3.10)

project(aparse LANGUAGES C)

option(APARSE_NO_ANSI_ESCAPE "Disable ANSI escape sequences in aparse's output" OFF)
option(APARSE_BUILD_WITH_ASAN "Build aparse with AddressSanitizer" OFF)
option(APARSE_BUILD_EXAMPLE "Build aparse's examples & tests" OFF)
# option(APARSE_LOG_STDOUT "Write aparse log messages to stdout" OFF)

function(target_add_asan target_name)
    if(NOT APARSE_BUILD_WITH_ASAN)
        return()
    endif()

    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "AddressSanitizer is not supported by '${CMAKE_C_COMPILER_ID}'")
    endif()

    target_compile_options(${target_name} PRIVATE
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:
            -fsanitize=address,leak
            -fno-omit-frame-pointer
        >
    )

    target_link_options(${target_name} PRIVATE
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:
            -fsanitize=address,leak
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
        >
    )
endfunction()

add_library(aparse STATIC 
    src/aparse_output.c
    src/aparse_parse.c
    src/aparse_utils.c
    src/aparse.c)

target_include_directories(aparse PUBLIC include)
target_compile_definitions(aparse PRIVATE
    $<$<BOOL:${APARSE_NO_ANSI_ESCAPE}>:APARSE_NO_ANSI_ESCAPE>)
if(UNIX AND NOT APPLE)
    target_compile_definitions(aparse PRIVATE
        _POSIX_C_SOURCE=200809L)
endif()
set_target_properties(aparse PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO)


target_add_asan(aparse)
target_add_strict_flags(aparse)

if(APARSE_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()
