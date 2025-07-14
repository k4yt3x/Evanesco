cmake_minimum_required(VERSION 3.19)
project(Evanesco LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(Evanesco
    WIN32
    src/main.cpp
    src/aboutdialog.cpp
    src/aboutdialog.h
    src/aboutdialog.ui
    src/mainwindow.cpp
    src/mainwindow.h
    src/mainwindow.ui
    src/procutils.cpp
    src/procutils.h
    src/windowhider.cpp
    src/windowhider.h
    "${CMAKE_CURRENT_SOURCE_DIR}/src/resources.rc"
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
