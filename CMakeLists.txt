cmake_minimum_required(VERSION 3.0.0)

project(toy-lisp-interpreter VERSION 0.1.0)

add_definitions(-D_DEFAULT_SOURCE)

add_library(lisp STATIC lisp.c lexical_scope.c compile.c compile2.c vm.c string_buffer.c text_stream.c)

add_executable(tests tests.c)
add_executable(main main.c)
add_executable(vmtest vmtest.c)

target_link_libraries(tests lisp)
target_link_libraries(main lisp)
target_link_libraries(vmtest lisp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
