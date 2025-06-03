cmake_minimum_required(VERSION 3.10)
project(aparse C) 


file(GLOB SOURCES src/*.c)

add_library(aparse STATIC ${SOURCES})
target_include_directories(aparse PUBLIC include)
set_property(TARGET aparse PROPERTY C_STANDARD 99)
