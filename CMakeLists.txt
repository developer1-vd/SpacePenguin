cmake_minimum_required(VERSION 3.21)

project(SpacePenguin
    VERSION 0.1.0
    DESCRIPTION "A secure, lightweight, and usable browser written in Qt"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets)

qt_add_executable(spacepenguin
    src/main.cpp
    src/browserwindow.cpp
    src/browserwindow.h
    src/browserpage.cpp
    src/browserpage.h
    src/startpageschemehandler.cpp
    src/startpageschemehandler.h
    src/urlresolver.cpp
    src/urlresolver.h
    src/resources.qrc
    html/start.html
)

target_link_libraries(spacepenguin PRIVATE Qt6::Widgets Qt6::WebEngineWidgets)
target_compile_definitions(spacepenguin PRIVATE
    SPACEPENGUIN_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(spacepenguin PRIVATE /W4 /permissive-)
else()
    target_compile_options(spacepenguin PRIVATE -Wall -Wextra)
endif()

option(SPACEPENGUIN_BUILD_TESTS "Build the SpacePenguin unit tests" ON)

if(SPACEPENGUIN_BUILD_TESTS)
    find_package(Qt6 6.8 QUIET COMPONENTS Test)
    if(TARGET Qt6::Test)
        enable_testing()
        qt_add_executable(tst_urlresolver
            tests/tst_urlresolver.cpp
            src/urlresolver.cpp
            src/urlresolver.h
        )
        target_link_libraries(tst_urlresolver PRIVATE Qt6::Test)
        target_include_directories(tst_urlresolver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME urlresolver COMMAND tst_urlresolver)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS spacepenguin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})