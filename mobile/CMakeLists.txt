cmake_minimum_required(VERSION 3.21)

# SpacePenguin for Android and iOS. QtWebEngine does not exist on phones, so this
# is its own app: a Qt Quick interface around QtWebView, which shows pages with
# the platform's engine (Android WebView, iOS WKWebView). It shares the address
# resolution, settings and YouTube/SponsorBlock code with the desktop app in ../src.
#
# It also builds on the desktop, where QtWebView uses QtWebEngine, for trying it
# out:  cmake -S mobile -B build-mobile && cmake --build build-mobile

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../CMakeLists.txt" _spacepenguin_version_line
     REGEX "^    VERSION [0-9.]+$")
string(REGEX MATCH "[0-9.]+" _spacepenguin_version "${_spacepenguin_version_line}")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../CMakeLists.txt" _spacepenguin_version_name_line
     REGEX "^set\\(SPACEPENGUIN_VERSION_NAME ")
string(REGEX MATCH "\"[^\"]+\"" SPACEPENGUIN_VERSION_NAME "${_spacepenguin_version_name_line}")
string(REPLACE "\"" "" SPACEPENGUIN_VERSION_NAME "${SPACEPENGUIN_VERSION_NAME}")

project(SpacePenguinMobile
    VERSION ${_spacepenguin_version}
    DESCRIPTION "SpacePenguin for Android and iOS"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# A desktop build finds the Qt that install-deps put in ../.qt/, as the desktop app
# does. Phone builds go through Qt's own qt-cmake for the target and skip this.
if(NOT CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_PREFIX_PATH AND NOT DEFINED ENV{CMAKE_PREFIX_PATH}
   AND NOT DEFINED Qt6_DIR)
    file(GLOB _spacepenguin_local_qt
        "${CMAKE_CURRENT_SOURCE_DIR}/../.qt/6.*/*/lib/cmake/Qt6WebView/Qt6WebViewConfig.cmake")
    list(FILTER _spacepenguin_local_qt EXCLUDE REGEX "/(android|ios|wasm)[^/]*/lib/")
    if(_spacepenguin_local_qt)
        list(SORT _spacepenguin_local_qt COMPARE NATURAL ORDER DESCENDING)
        list(GET _spacepenguin_local_qt 0 _spacepenguin_local_qt)
        cmake_path(GET _spacepenguin_local_qt PARENT_PATH _spacepenguin_local_qt)
        cmake_path(APPEND _spacepenguin_local_qt ".." ".." "..")
        cmake_path(NORMAL_PATH _spacepenguin_local_qt)
        message(STATUS "Using Qt from ${_spacepenguin_local_qt}")
        list(APPEND CMAKE_PREFIX_PATH "${_spacepenguin_local_qt}")
    endif()
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 WebView)
qt_standard_project_setup(REQUIRES 6.8)
include(GNUInstallDirs)

set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../src)

qt_add_executable(spacepenguin_mobile
    main.cpp
    ${SHARED}/preferences.cpp
    ${SHARED}/preferences.h
    ${SHARED}/sponsorblock.cpp
    ${SHARED}/sponsorblock.h
    ${SHARED}/urlresolver.cpp
    ${SHARED}/urlresolver.h
    ${SHARED}/youtubescripts.cpp
    ${SHARED}/youtubescripts.h
)
set_target_properties(spacepenguin_mobile PROPERTIES OUTPUT_NAME SpacePenguin)
target_include_directories(spacepenguin_mobile PRIVATE ${SHARED} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(spacepenguin_mobile PRIVATE SPACEPENGUIN_VERSION="${SPACEPENGUIN_VERSION_NAME}")
target_link_libraries(spacepenguin_mobile PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::WebView)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(spacepenguin_mobile
    URI SpacePenguin
    VERSION 1.0
    # Beside the program, which is also called SpacePenguin, it needs a folder of its own.
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/SpacePenguin
    QML_FILES
        qml/Main.qml
        qml/BrowserTab.qml
        qml/StartPage.qml
        qml/SheetPage.qml
        qml/LinkList.qml
        qml/MenuRow.qml
        qml/IconButton.qml
        qml/Icon.qml
        qml/Theme.qml
    SOURCES
        browserbackend.cpp
        browserbackend.h
)

qt_add_resources(spacepenguin_mobile "assets"
    PREFIX "/"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/..
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../filters/default.txt
)
qt_add_resources(spacepenguin_mobile "icons"
    PREFIX "/icons"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/../packaging/icons
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../packaging/icons/spacepenguin-256.png
)

if(ANDROID)
    # The manifest, launcher icons and app name live in android/.
    set_target_properties(spacepenguin_mobile PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
        QT_ANDROID_VERSION_NAME ${SPACEPENGUIN_VERSION_NAME}
        QT_ANDROID_VERSION_CODE 1
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 34)
elseif(IOS)
    # Qt writes the Info.plist (with its launch screen) from these properties.
    set_target_properties(spacepenguin_mobile PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME SpacePenguin
        MACOSX_BUNDLE_GUI_IDENTIFIER org.spacepenguin.SpacePenguin
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER org.spacepenguin.SpacePenguin
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon)
    target_sources(spacepenguin_mobile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

# On Android the app is a library inside the APK, which the apk target builds.
if(NOT ANDROID)
    install(TARGETS spacepenguin_mobile
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
