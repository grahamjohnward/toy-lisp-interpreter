cmake_minimum_required(VERSION 3.0.0)

project(toy-lisp-interpreter VERSION 0.1.0)

add_definitions(-D_DEFAULT_SOURCE)

add_library(lisp STATIC lisp.c string_buffer.c)

add_executable(tests tests.c)
add_executable(main main.c)

target_link_libraries(tests lisp)
target_link_libraries(main lisp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
