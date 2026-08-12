add_library(ds-lyrics-dock SHARED
    lyricsdockapplet.cpp
    lyricsdockapplet.h
)

target_link_libraries(ds-lyrics-dock
    PRIVATE
        Dde::Shell
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
)

ds_install_package(
    PACKAGE org.deepin.ds.lyrics-dock
    TARGET ds-lyrics-dock
)
