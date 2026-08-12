add_library(lyrics-settings-viewmodel STATIC
    settingsviewmodel.cpp
    settingsviewmodel.h
)

add_library(Lyrics::SettingsViewModel ALIAS lyrics-settings-viewmodel)

target_include_directories(lyrics-settings-viewmodel
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(lyrics-settings-viewmodel
    PUBLIC
        Qt6::Core
        Qt6::DBus
)

add_library(lyrics-settings-window STATIC
    settingswindow.cpp
    settingswindow.h
)

add_library(Lyrics::SettingsWindow ALIAS lyrics-settings-window)

target_include_directories(lyrics-settings-window
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(lyrics-settings-window
    PUBLIC
        Lyrics::SettingsViewModel
        Lyrics::Ui
        Qt6::Core
        Qt6::DBus
        Qt6::Widgets
        Dtk6::Core
        Dtk6::Gui
        Dtk6::Widget
)

add_executable(deepin-lyrics-settings
    main.cpp
)

qt_add_translation(SETTINGS_QM_FILES
    translations/deepin-lyrics-settings_zh_CN.ts
)
set_source_files_properties(${SETTINGS_QM_FILES} PROPERTIES
    QT_RESOURCE_ALIAS "deepin-lyrics-settings_zh_CN.qm"
)
qt_add_resources(deepin-lyrics-settings settings-translations
    PREFIX "/translations"
    FILES ${SETTINGS_QM_FILES}
)

target_link_libraries(deepin-lyrics-settings
    PRIVATE
        Lyrics::Core
        Lyrics::SettingsViewModel
        Lyrics::SettingsWindow
        Lyrics::Ui
        Lyrics::Version
        Qt6::Core
        Qt6::DBus
        Qt6::Widgets
        Dtk6::Core
        Dtk6::Gui
        Dtk6::Widget
)

install(TARGETS deepin-lyrics-settings
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
