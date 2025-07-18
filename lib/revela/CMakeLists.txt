cmake_minimum_required(VERSION 3.19)

project(Revela LANGUAGES CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "This project can only be built with Clang.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_NAME "64")
else()
    set(ARCH_NAME "32")
endif()

add_library(Revela SHARED
    revela.cpp
)

set_target_properties(Revela PROPERTIES
    OUTPUT_NAME "Revela${ARCH_NAME}"
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../build/Revela/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../build/Revela/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../build/Revela/"
)

target_compile_definitions(Revela PRIVATE
    REVELA_EXPORTS
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:_WIN64>
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>:_WIN32>
)

target_link_libraries(Revela PRIVATE
    user32
    kernel32
    advapi32
)

target_compile_options(Revela PRIVATE
    $<$<CONFIG:Release>:-Os>
)

target_link_options(Revela PRIVATE
    $<$<CONFIG:Release>:/OPT:REF>
    $<$<CONFIG:Release>:/OPT:ICF>
)
