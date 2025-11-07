cmake_minimum_required(VERSION 3.10)
project(aparse C) 


file(GLOB SOURCES src/*.c)

add_library(aparse STATIC ${SOURCES})
target_include_directories(aparse PUBLIC include)
set_property(TARGET aparse PROPERTY C_STANDARD 99)

project(example C)
add_executable(example example/example.c)
target_link_libraries(example PRIVATE aparse)

project(tests C)
add_executable(tests example/tests.c)
target_link_libraries(tests PRIVATE aparse)