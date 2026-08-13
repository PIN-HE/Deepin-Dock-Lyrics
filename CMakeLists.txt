cmake_minimum_required(VERSION 3.16)

project(deepin_dock_lyrics VERSION 1.0.0.2 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DEEPIN_DOCK_LYRICS_BUILD_TESTS "Build Deepin Dock Lyrics tests" ON)
option(DEEPIN_DOCK_LYRICS_ENABLE_PIPEWIRE
       "Enable the optional PipeWire audio visualizer backend when available" ON)

configure_file(
    cmake/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/deepinlyrics/version.h
    @ONLY
)

add_library(lyrics-version INTERFACE)
add_library(Lyrics::Version ALIAS lyrics-version)
target_include_directories(lyrics-version
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
)

find_package(Qt6 REQUIRED COMPONENTS Core DBus LinguistTools Network Sql Widgets Qml Quick Test)
find_package(Dtk6 REQUIRED COMPONENTS Core Gui Widget)
find_package(Dtk6DConfig REQUIRED)
find_package(DDEShell REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(OpenCC REQUIRED IMPORTED_TARGET opencc)
if(DEEPIN_DOCK_LYRICS_ENABLE_PIPEWIRE)
    pkg_check_modules(PipeWire QUIET IMPORTED_TARGET libpipewire-0.3)
endif()

if(PipeWire_FOUND)
    message(STATUS "PipeWire audio visualizer backend is enabled")
else()
    message(STATUS "PipeWire audio visualizer backend is unavailable; lyrics remain fully functional")
endif()

add_subdirectory(common/lyrics-core)
add_subdirectory(common/lyrics-logging)
add_subdirectory(common/lyrics-ui)
add_subdirectory(daemon/lyrics-dockd)
add_subdirectory(apps/lyrics-settings)
add_subdirectory(dock-applet)

install(FILES systemd/lyrics-dockd.service
    DESTINATION lib/systemd/user
)
install(FILES debian/org.deepin.LyricsDock1.service
    DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services
)

if(DEEPIN_DOCK_LYRICS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
