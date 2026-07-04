cmake_minimum_required(VERSION 3.10)
project(aparse C) 

option(APARSE_BUILD_ASAN    "Build aparse with AddressSanitizer" OFF)
option(APARSE_BUILD_EXAMPLE "Build aparse's example & tests" OFF)

function(target_add_asan target_name)
    if(NOT APARSE_BUILD_ASAN)
        return()
    endif()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE
            -fsanitize=address
            -fno-omit-frame-pointer
        )

        target_link_options(${target_name} PRIVATE
            -fsanitize=address
        )
    endif()
endfunction()


set(APARSE_BUILD_DEBUG FALSE)
if(CMAKE_BUILD_TYPE MATCHES "[Dd]ebug"
        OR CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
    set(APARSE_BUILD_DEBUG TRUE)
endif()

if(APARSE_BUILD_ASAN)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        if(NOT APARSE_BUILD_DEBUG)
            message(FATAL_ERROR "APARSE_BUILD_ASAN MUST be built with Debug or RelWithDebInfo")
        endif()
    else()
        message(FATAL_ERROR "AddressSanitizer was not supported by '${CMAKE_C_COMPILER_ID}'")
    endif()
endif()

function(target_add_strict_flags target_name)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        if(APARSE_BUILD_DEBUG)
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

                    # Modern MSVC support this, also intended behaviour
                    -Wno-gnu-zero-variadic-macro-arguments
                >
            )
        endif()
    endif()
endfunction()


set(APARSE_SOURCES
    src/aparse.c
    src/aparse_list.c)

add_library(aparse STATIC ${APARSE_SOURCES})
target_include_directories(aparse SYSTEM PUBLIC include)
target_compile_features(aparse PRIVATE c_std_11)
target_add_asan(aparse)
target_add_strict_flags(aparse)

if(APARSE_BUILD_EXAMPLE)
    add_executable(example example/example.c)
    target_link_libraries(example PRIVATE aparse)
    target_add_asan(example)

    add_executable(subcmd example/subcmd_example.c)
    target_link_libraries(subcmd PRIVATE aparse)
    if(UNIX)
        target_link_libraries(subcmd PRIVATE m)
    endif()
    
    target_add_asan(subcmd)

    add_executable(tests example/tests.c)
    target_link_libraries(tests PRIVATE aparse)
    target_add_asan(tests)
endif()
