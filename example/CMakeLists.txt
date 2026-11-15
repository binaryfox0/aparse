cmake_minimum_required(VERSION 3.16)
add_executable(example example.c)
target_link_libraries(example PRIVATE aparse)
target_add_asan(example)