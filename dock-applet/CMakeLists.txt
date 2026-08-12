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

set(DOCK_TRANSLATION_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/translations/org.deepin.ds.lyrics-dock_zh_CN.ts
)
qt_add_translation(DOCK_TRANSLATION_QM ${DOCK_TRANSLATION_SOURCE})
add_custom_target(org.deepin.ds.lyrics-dock_translation ALL
    DEPENDS ${DOCK_TRANSLATION_QM}
)
add_dependencies(org.deepin.ds.lyrics-dock_package
    org.deepin.ds.lyrics-dock_translation
)
add_custom_command(TARGET org.deepin.ds.lyrics-dock_package POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            ${PROJECT_BINARY_DIR}/packages/org.deepin.ds.lyrics-dock/translations
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DOCK_TRANSLATION_QM}
            ${PROJECT_BINARY_DIR}/packages/org.deepin.ds.lyrics-dock/translations/
)
install(FILES ${DOCK_TRANSLATION_QM}
    DESTINATION ${DDE_SHELL_TRANSLATION_INSTALL_DIR}/org.deepin.ds.lyrics-dock/translations
)
