add_library(lyrics-dock-viewmodel STATIC
    lyricsdockviewmodel.cpp
    lyricsdockviewmodel.h
)

add_library(Lyrics::DockViewModel ALIAS lyrics-dock-viewmodel)

target_include_directories(lyrics-dock-viewmodel
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(lyrics-dock-viewmodel
    PUBLIC
        Qt6::Core
        Qt6::DBus
)

add_library(ds-lyrics-dock SHARED
    lyricsdockapplet.cpp
    lyricsdockapplet.h
)

target_link_libraries(ds-lyrics-dock
    PRIVATE
        Lyrics::Ui
        Lyrics::DockViewModel
        Dde::Shell
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
)

ds_install_package(
    PACKAGE org.deepin.ds.lyrics-dock
    TARGET ds-lyrics-dock
)
