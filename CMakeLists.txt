cmake_minimum_required(VERSION 3.19)

project(Evanesco LANGUAGES CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "This project can only be built with Clang.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(Evanesco
    WIN32
    src/aboutdialog.cpp
    src/aboutdialog.h
    src/aboutdialog.ui
    src/autohider.cpp
    src/autohider.h
    src/hashutils.cpp
    src/hashutils.h
    src/ipcutils.cpp
    src/ipcutils.h
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/mainwindow.ui
    src/prefdialog.cpp
    src/prefdialog.h
    src/prefdialog.ui
    src/procutils.cpp
    src/procutils.h
    src/randutils.cpp
    src/randutils.h
    src/resources.rc
    src/settings.cpp
    src/settings.h
    src/tempfile.cpp
    src/tempfile.h
    src/windowhider.cpp
    src/windowhider.h
    src/version.h
)

qt_add_resources(Evanesco "resources"
    PREFIX "/"
    FILES resources/evanesco.ico
)

target_compile_options(Evanesco PRIVATE
    $<$<CONFIG:Release>:-Os>
)
target_link_options(Evanesco PRIVATE
    $<$<CONFIG:Release>:-s>
)

target_link_libraries(Evanesco
    PRIVATE
        Qt::Core
        Qt::Widgets
        wbemuuid
)

include(GNUInstallDirs)

install(TARGETS Evanesco
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Evanesco
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)

install(SCRIPT ${deploy_script})
