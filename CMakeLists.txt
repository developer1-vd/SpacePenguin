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

include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets)

qt_add_executable(spacepenguin
    src/main.cpp
    src/aboutpages.cpp
    src/aboutpages.h
    src/browserwindow.cpp
    src/browserwindow.h
    src/browserpage.cpp
    src/browserpage.h
    src/adblocker.cpp
    src/adblocker.h
    src/cosmeticfilters.cpp
    src/cosmeticfilters.h
    src/extensionsdialog.cpp
    src/extensionsdialog.h
    src/theme.cpp
    src/theme.h
    src/userextensions.cpp
    src/userextensions.h
    src/profiles.cpp
    src/profiles.h
    src/startpageschemehandler.cpp
    src/startpageschemehandler.h
    src/urlresolver.cpp
    src/urlresolver.h
    src/resources.qrc
    html/start.html
    filters/default.txt
)

target_link_libraries(spacepenguin PRIVATE Qt6::Widgets Qt6::WebEngineWidgets)
target_compile_definitions(spacepenguin PRIVATE
    SPACEPENGUIN_VERSION="${PROJECT_VERSION}"
    SPACEPENGUIN_BUILD_TYPE="$<CONFIG>")

if(MSVC)
    target_compile_options(spacepenguin PRIVATE /W4 /permissive-)
else()
    target_compile_options(spacepenguin PRIVATE -Wall -Wextra)
endif()

# ---- Platform integration ---------------------------------------------------

if(WIN32)
    # Launch as a GUI application so no console window appears behind the browser.
    set_target_properties(spacepenguin PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
    set_target_properties(spacepenguin PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "SpacePenguin"
        MACOSX_BUNDLE_BUNDLE_NAME "SpacePenguin"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.spacepenguin.SpacePenguin"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in")
endif()

if(NOT APPLE)
    # An installed copy has to find QtWebEngineProcess, plugins and translations.
    # Derive the Qt layout from the qmake target so this works for any Qt build.
    get_target_property(SPACEPENGUIN_QMAKE Qt6::qmake IMPORTED_LOCATION)
    if(SPACEPENGUIN_QMAKE)
        get_filename_component(SPACEPENGUIN_QT_BIN_DIR "${SPACEPENGUIN_QMAKE}" DIRECTORY)
        get_filename_component(SPACEPENGUIN_QT_ROOT "${SPACEPENGUIN_QT_BIN_DIR}" DIRECTORY)

        set(SPACEPENGUIN_QT_LIB_DIR "")
        foreach(candidate IN ITEMS "${SPACEPENGUIN_QT_ROOT}/lib64" "${SPACEPENGUIN_QT_ROOT}/lib")
            if(EXISTS "${candidate}")
                set(SPACEPENGUIN_QT_LIB_DIR "${candidate}")
                break()
            endif()
        endforeach()
        if(NOT SPACEPENGUIN_QT_LIB_DIR)
            set(SPACEPENGUIN_QT_LIB_DIR "${SPACEPENGUIN_QT_BIN_DIR}")
        endif()

        set(SPACEPENGUIN_QT_EXEC_DIR "${SPACEPENGUIN_QT_ROOT}/libexec")
        if(NOT EXISTS "${SPACEPENGUIN_QT_EXEC_DIR}")
            set(SPACEPENGUIN_QT_EXEC_DIR "${SPACEPENGUIN_QT_BIN_DIR}")
        endif()

        configure_file(cmake/qt.conf.in "${CMAKE_CURRENT_BINARY_DIR}/qt.conf" @ONLY)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qt.conf"
            DESTINATION ${CMAKE_INSTALL_BINDIR})
    else()
        message(WARNING
            "Qt6::qmake location unknown; skipping qt.conf generation. "
            "Installed copies will not find QtWebEngineProcess.")
    endif()
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

        qt_add_executable(tst_aboutpages
            tests/tst_aboutpages.cpp
            src/aboutpages.cpp
            src/aboutpages.h
        )
        target_link_libraries(tst_aboutpages PRIVATE Qt6::Test)
        target_include_directories(tst_aboutpages PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME aboutpages COMMAND tst_aboutpages)

        qt_add_executable(tst_adblocker
            tests/tst_adblocker.cpp
            src/adblocker.cpp
            src/adblocker.h
            src/cosmeticfilters.cpp
            src/cosmeticfilters.h
        )
        target_link_libraries(tst_adblocker PRIVATE Qt6::Test Qt6::WebEngineCore)
        target_include_directories(tst_adblocker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_compile_definitions(tst_adblocker PRIVATE
            SPACEPENGUIN_TEST_FILTER_LIST="${CMAKE_CURRENT_SOURCE_DIR}/filters/default.txt")
        add_test(NAME adblocker COMMAND tst_adblocker)

        qt_add_executable(tst_userextensions
            tests/tst_userextensions.cpp
            src/userextensions.cpp
            src/userextensions.h
        )
        target_link_libraries(tst_userextensions PRIVATE Qt6::Test Qt6::WebEngineCore)
        target_include_directories(tst_userextensions PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME userextensions COMMAND tst_userextensions)
    endif()
endif()

include(GNUInstallDirs)

include(GNUInstallDirs)

if(APPLE)
    install(TARGETS spacepenguin
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    install(TARGETS spacepenguin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()