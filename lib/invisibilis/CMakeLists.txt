cmake_minimum_required(VERSION 3.19)

project(Invisibilis LANGUAGES CXX)

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

add_library(Invisibilis SHARED
    src/invisibilis.cpp
    ../../src/hashutils.cpp
    ../../src/ipcutils.cpp
)

set_target_properties(Invisibilis PROPERTIES
    OUTPUT_NAME "Invisibilis${ARCH_NAME}"
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../build/Invisibilis/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../build/Invisibilis/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../build/Invisibilis/"
)

target_include_directories(Invisibilis PRIVATE
    ${CMAKE_SOURCE_DIR}/../../src
)

target_compile_definitions(Invisibilis PRIVATE
    INVISIBILIS_EXPORTS
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:_WIN64>
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>:_WIN32>
)

target_link_libraries(Invisibilis PRIVATE
    user32
    kernel32
    advapi32
)

target_compile_options(Invisibilis PRIVATE
    $<$<CONFIG:Release>:-Os>
)

target_link_options(Invisibilis PRIVATE
    $<$<CONFIG:Release>:/OPT:REF>
    $<$<CONFIG:Release>:/OPT:ICF>
)
