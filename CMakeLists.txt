cmake_minimum_required(VERSION 3.16)

project(deepin_dock_lyrics VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DEEPIN_DOCK_LYRICS_BUILD_TESTS "Build Deepin Dock Lyrics tests" ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus LinguistTools Network Sql Widgets Qml Quick Test)
find_package(Dtk6 REQUIRED COMPONENTS Core Gui Widget)
find_package(Dtk6DConfig REQUIRED)
find_package(DDEShell REQUIRED)

add_subdirectory(common/lyrics-core)
add_subdirectory(common/lyrics-logging)
add_subdirectory(common/lyrics-ui)
add_subdirectory(daemon/lyrics-dockd)
add_subdirectory(apps/lyrics-settings)
add_subdirectory(dock-applet)

if(DEEPIN_DOCK_LYRICS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
