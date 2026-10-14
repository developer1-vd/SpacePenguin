cmake_minimum_required(VERSION 3.21)

project(SpacePenguin
    VERSION 0.1.0
    DESCRIPTION "A secure, lightweight, and usable browser written in Qt"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

get_property(SPACEPENGUIN_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT SPACEPENGUIN_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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
    src/webview.cpp
    src/webview.h
    src/icons.cpp
    src/icons.h
    src/preferences.cpp
    src/preferences.h
    src/historydialog.cpp
    src/historydialog.h
    src/downloadsdialog.cpp
    src/downloadsdialog.h
    src/bookmarksdialog.cpp
    src/bookmarksdialog.h
    src/adblocker.cpp
    src/adblocker.h
    src/cosmeticfilters.cpp
    src/cosmeticfilters.h
    src/downloadmanager.cpp
    src/downloadmanager.h
    src/bookmarkmanager.cpp
    src/bookmarkmanager.h
    src/cookiemanager.cpp
    src/cookiemanager.h
    src/datasavermanager.cpp
    src/datasavermanager.h
    src/historymanager.cpp
    src/historymanager.h
    src/settingsdialog.cpp
    src/settingsdialog.h
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
    set_target_properties(spacepenguin PROPERTIES OUTPUT_NAME "SpacePenguin")
    # An installed copy has to find QtWebEngineProcess, plugins, resources and
    # translations. Qt publishes its own layout, so use that rather than guessing.
    if(DEFINED QT6_INSTALL_PREFIX)
        set(SPACEPENGUIN_QT_ROOT "${QT6_INSTALL_PREFIX}")
        foreach(kind IN ITEMS BINS LIBS PLUGINS LIBEXECS DATA TRANSLATIONS)
            if(IS_ABSOLUTE "${QT6_INSTALL_${kind}}")
                set(SPACEPENGUIN_QT_${kind} "${QT6_INSTALL_${kind}}")
            else()
                set(SPACEPENGUIN_QT_${kind} "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_${kind}}")
            endif()
        endforeach()

        configure_file(cmake/qt.conf.in "${CMAKE_CURRENT_BINARY_DIR}/qt.conf.install" @ONLY)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qt.conf.install"
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            RENAME qt.conf)
    else()
        message(WARNING
            "Qt install layout unknown; skipping qt.conf generation. "
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

        qt_add_executable(tst_startpage
            tests/tst_startpage.cpp
            src/startpageschemehandler.cpp
            src/startpageschemehandler.h
            src/preferences.cpp
            src/preferences.h
            src/theme.cpp
            src/theme.h
            src/urlresolver.cpp
            src/urlresolver.h
        )
        target_link_libraries(tst_startpage PRIVATE Qt6::Test Qt6::Widgets Qt6::WebEngineCore)
        target_include_directories(tst_startpage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_compile_definitions(tst_startpage PRIVATE
            SPACEPENGUIN_START_PAGE="${CMAKE_CURRENT_SOURCE_DIR}/html/start.html")
        add_test(NAME startpage COMMAND tst_startpage)
    endif()
endif()

if(APPLE)
    install(TARGETS spacepenguin
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    install(TARGETS spacepenguin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()