cmake_minimum_required(VERSION 3.0.0)
project(toy-lisp-interpreter VERSION 0.1.0)

include(CTest)
enable_testing()

add_definitions(-D_DEFAULT_SOURCE)

add_executable(toy-lisp-interpreter lisp.c string_buffer.c)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
