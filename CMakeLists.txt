cmake_minimum_required(VERSION 3.10)
project(aparse C) 

option(APARSE_BUILD_EXAMPLE "Build aparse's example & tests" OFF)

file(GLOB SOURCES src/*.c)

add_library(aparse STATIC ${SOURCES})
target_include_directories(aparse PUBLIC include)
set_property(TARGET aparse PROPERTY C_STANDARD 99)

if(APARSE_BUILD_EXAMPLE)
    function(target_add_asan target)
        if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            message(STATUS "AddressSanitizer support was found")
            target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address)
        else()
            message(WARNING "AddressSanitizer support was not found")
        endif()
    endfunction()

    add_executable(example example/example.c)
    target_link_libraries(example PRIVATE aparse)
    target_add_asan(example)

    add_executable(tests example/tests.c)
    target_link_libraries(tests PRIVATE aparse)
    target_add_asan(tests)
endif()
