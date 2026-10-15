cmake_minimum_required(VERSION 3.21)

# Must be set before project() to reach the compiler and the Info.plist.
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

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

# Windows apps keep the executable and its Qt DLLs side by side in the install root.
if(WIN32)
    set(CMAKE_INSTALL_BINDIR "." CACHE PATH "Executable install directory")
endif()

# The sources contain UTF-8 text ("…", "—"); MSVC otherwise reads them in the
# system code page and garbles every such string.
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
endif()

include(GNUInstallDirs)

# Qt installed by install-deps.sh / install-deps.ps1 lives in .qt/ and is used
# automatically when no other Qt was given.
option(SPACEPENGUIN_USE_LOCAL_QT "Use a Qt installed into .qt/ by install-deps" ON)
if(SPACEPENGUIN_USE_LOCAL_QT AND NOT CMAKE_PREFIX_PATH AND NOT DEFINED ENV{CMAKE_PREFIX_PATH}
   AND NOT DEFINED Qt6_DIR)
    file(GLOB SPACEPENGUIN_LOCAL_QT_CONFIGS
        "${CMAKE_CURRENT_SOURCE_DIR}/.qt/6.*/*/lib/cmake/Qt6/Qt6Config.cmake")
    if(SPACEPENGUIN_LOCAL_QT_CONFIGS)
        list(SORT SPACEPENGUIN_LOCAL_QT_CONFIGS COMPARE NATURAL ORDER DESCENDING)
        list(GET SPACEPENGUIN_LOCAL_QT_CONFIGS 0 SPACEPENGUIN_LOCAL_QT_CONFIG)
        cmake_path(GET SPACEPENGUIN_LOCAL_QT_CONFIG PARENT_PATH SPACEPENGUIN_LOCAL_QT)
        cmake_path(APPEND SPACEPENGUIN_LOCAL_QT ".." ".." "..")
        cmake_path(NORMAL_PATH SPACEPENGUIN_LOCAL_QT)
        string(REGEX REPLACE "/$" "" SPACEPENGUIN_LOCAL_QT "${SPACEPENGUIN_LOCAL_QT}")
        message(STATUS "Using Qt from ${SPACEPENGUIN_LOCAL_QT}")
        list(APPEND CMAKE_PREFIX_PATH "${SPACEPENGUIN_LOCAL_QT}")
    endif()
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Widgets WebEngineWidgets)

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
    src/shortcuts.cpp
    src/shortcuts.h
    src/singleinstance.cpp
    src/singleinstance.h
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

target_link_libraries(spacepenguin PRIVATE Qt6::Network Qt6::Widgets Qt6::WebEngineWidgets)
target_compile_definitions(spacepenguin PRIVATE
    SPACEPENGUIN_VERSION="${PROJECT_VERSION}"
    SPACEPENGUIN_BUILD_TYPE="$<CONFIG>")

if(MSVC)
    target_compile_options(spacepenguin PRIVATE /W4 /permissive-)
else()
    target_compile_options(spacepenguin PRIVATE -Wall -Wextra)
endif()

# ---- Platform integration ---------------------------------------------------

set_target_properties(spacepenguin PROPERTIES OUTPUT_NAME "SpacePenguin")

if(WIN32)
    # Launch as a GUI application so no console window appears behind the browser,
    # and embed the icon and version information Explorer shows.
    set_target_properties(spacepenguin PROPERTIES WIN32_EXECUTABLE TRUE)
    set(SPACEPENGUIN_ICO "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/spacepenguin.ico")
    file(TO_CMAKE_PATH "${SPACEPENGUIN_ICO}" SPACEPENGUIN_ICO)
    configure_file(packaging/windows/spacepenguin.rc.in
        "${CMAKE_CURRENT_BINARY_DIR}/spacepenguin.rc" @ONLY)
    target_sources(spacepenguin PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/spacepenguin.rc")
endif()

if(APPLE)
    set(SPACEPENGUIN_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/SpacePenguin.icns")
    set_source_files_properties("${SPACEPENGUIN_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(spacepenguin PRIVATE "${SPACEPENGUIN_ICNS}")
    set_target_properties(spacepenguin PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "SpacePenguin"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.spacepenguin.SpacePenguin"
        MACOSX_BUNDLE_ICON_FILE "SpacePenguin.icns"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in")
endif()

if(UNIX AND NOT APPLE)
    # A Linux install uses the system's (or a given) Qt in place, so it ships a
    # qt.conf that points at that Qt's layout: QtWebEngineProcess, plugins,
    # resources and translations are then found without environment variables.
    if(DEFINED QT6_INSTALL_PREFIX)
        set(SPACEPENGUIN_QT_ROOT "${QT6_INSTALL_PREFIX}")
        foreach(kind IN ITEMS BINS LIBS PLUGINS LIBEXECS DATA TRANSLATIONS)
            if(IS_ABSOLUTE "${QT6_INSTALL_${kind}}")
                set(SPACEPENGUIN_QT_${kind} "${QT6_INSTALL_${kind}}")
            else()
                set(SPACEPENGUIN_QT_${kind} "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_${kind}}")
            endif()
        endforeach()

        option(SPACEPENGUIN_INSTALL_QT_CONF
            "Install a qt.conf pointing at the Qt this was built against" ON)
        if(SPACEPENGUIN_INSTALL_QT_CONF)
            configure_file(cmake/qt.conf.in "${CMAKE_CURRENT_BINARY_DIR}/qt.conf.install" @ONLY)
            install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qt.conf.install"
                DESTINATION ${CMAKE_INSTALL_BINDIR}
                RENAME qt.conf)
        endif()
    else()
        message(WARNING
            "Qt install layout unknown; skipping qt.conf generation. "
            "Installed copies will not find QtWebEngineProcess.")
    endif()

    # Desktop integration: launcher entry, AppStream metadata and icons.
    install(FILES packaging/linux/org.spacepenguin.SpacePenguin.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    string(TIMESTAMP SPACEPENGUIN_RELEASE_DATE "%Y-%m-%d" UTC)
    configure_file(packaging/linux/org.spacepenguin.SpacePenguin.metainfo.xml
        "${CMAKE_CURRENT_BINARY_DIR}/org.spacepenguin.SpacePenguin.metainfo.xml" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.spacepenguin.SpacePenguin.metainfo.xml"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    foreach(size IN ITEMS 16 32 48 64 128 256 512)
        install(FILES packaging/icons/spacepenguin-${size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
            RENAME org.spacepenguin.SpacePenguin.png)
    endforeach()
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
            src/shortcuts.cpp
            src/shortcuts.h
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

        qt_add_executable(tst_shortcuts
            tests/tst_shortcuts.cpp
            src/shortcuts.cpp
            src/shortcuts.h
        )
        target_link_libraries(tst_shortcuts PRIVATE Qt6::Test Qt6::Gui)
        target_include_directories(tst_shortcuts PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME shortcuts COMMAND tst_shortcuts)

        qt_add_executable(tst_singleinstance
            tests/tst_singleinstance.cpp
            src/singleinstance.cpp
            src/singleinstance.h
        )
        target_link_libraries(tst_singleinstance PRIVATE Qt6::Test Qt6::Network)
        target_include_directories(tst_singleinstance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME singleinstance COMMAND tst_singleinstance)
    endif()
endif()

# ---- Tools ---------------------------------------------------------------------

option(SPACEPENGUIN_BUILD_TOOLS "Build the icon generator (tools/generate-icons.cpp)" OFF)
if(SPACEPENGUIN_BUILD_TOOLS)
    qt_add_executable(spacepenguin-generate-icons
        tools/generate-icons.cpp
        src/icons.cpp
        src/icons.h
        src/theme.cpp
        src/theme.h
    )
    target_link_libraries(spacepenguin-generate-icons PRIVATE Qt6::Widgets)
    target_include_directories(spacepenguin-generate-icons PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

# ---- Install and deploy -----------------------------------------------------

install(TARGETS spacepenguin
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# On Windows and macOS, `cmake --install` also runs windeployqt / macdeployqt, so
# the install prefix holds a self-contained app: Qt libraries, plugins,
# QtWebEngineProcess and its resources. Linux uses the system Qt (see qt.conf).
option(SPACEPENGUIN_DEPLOY_QT
    "Copy Qt into the install prefix on Windows and macOS (off for Nix and other package managers)" ON)
if((WIN32 OR APPLE) AND SPACEPENGUIN_DEPLOY_QT AND COMMAND qt_generate_deploy_app_script)
    qt_generate_deploy_app_script(
        TARGET spacepenguin
        OUTPUT_SCRIPT SPACEPENGUIN_DEPLOY_SCRIPT
        NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT "${SPACEPENGUIN_DEPLOY_SCRIPT}")
endif()

# ---- Linux distribution packages (CPack) ------------------------------------
#
# `cpack -G DEB` / `cpack -G RPM` / `cpack -G TGZ` in a build tree configured with
# -DCMAKE_INSTALL_PREFIX=/usr -DSPACEPENGUIN_INSTALL_QT_CONF=OFF against the
# distribution's own Qt (6.8+) gives a native package whose dependencies are
# worked out from the binary. build-linux-packages.sh does all of that.

if(UNIX AND NOT APPLE)
    set(CPACK_PACKAGE_NAME "spacepenguin")
    set(CPACK_PACKAGE_VENDOR "SpacePenguin")
    set(CPACK_PACKAGE_CONTACT "SpacePenguin <https://github.com/developer1-vd/SpacePenguin/issues>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_PACKAGE_DESCRIPTION
        "SpacePenguin is a small web browser built on Qt 6 and QtWebEngine. It blocks ads with Adblock Plus style filter lists, runs JavaScript user scripts, keeps private windows off the disk, and never phones home.")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/developer1-vd/SpacePenguin")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_STRIP_FILES ON)
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")

    # Debian / Ubuntu
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SECTION "web")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_PROVIDES "www-browser")
    # Loaded at run time, so the shared-library scan cannot see them.
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins, libqt6webenginecore6-bin")

    # Fedora / openSUSE / RHEL
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    set(CPACK_RPM_PACKAGE_PROVIDES "webclient")
    set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        /usr/share/applications /usr/share/metainfo /usr/share/icons
        /usr/share/icons/hicolor /usr/share/icons/hicolor/16x16 /usr/share/icons/hicolor/16x16/apps
        /usr/share/icons/hicolor/32x32 /usr/share/icons/hicolor/32x32/apps
        /usr/share/icons/hicolor/48x48 /usr/share/icons/hicolor/48x48/apps
        /usr/share/icons/hicolor/64x64 /usr/share/icons/hicolor/64x64/apps
        /usr/share/icons/hicolor/128x128 /usr/share/icons/hicolor/128x128/apps
        /usr/share/icons/hicolor/256x256 /usr/share/icons/hicolor/256x256/apps
        /usr/share/icons/hicolor/512x512 /usr/share/icons/hicolor/512x512/apps)

    include(CPack)
endif()
