add_executable(lyrics-core-test
    test_lyricscore.cpp
)

target_link_libraries(lyrics-core-test
    PRIVATE
        Lyrics::Core
        Qt6::Core
        Qt6::Test
)

add_test(NAME lyrics-core-test COMMAND lyrics-core-test)

add_executable(lrc-parser-test
    test_lrcparser.cpp
)

target_compile_definitions(lrc-parser-test PRIVATE
    TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_link_libraries(lrc-parser-test
    PRIVATE
        Lyrics::Core
        Qt6::Core
        Qt6::Test
)

add_test(NAME lrc-parser-test COMMAND lrc-parser-test)

add_executable(lyric-sync-test
    test_lyricsync.cpp
)

target_link_libraries(lyric-sync-test
    PRIVATE
        Lyrics::Core
        Qt6::Core
        Qt6::Test
)

add_test(NAME lyric-sync-test COMMAND lyric-sync-test)

add_executable(lyrics-ui-test
    test_lyricstokens.cpp
)

target_link_libraries(lyrics-ui-test
    PRIVATE
        Lyrics::Ui
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Test
        Dtk6::Gui
)

add_test(NAME lyrics-ui-test COMMAND lyrics-ui-test)
set_tests_properties(lyrics-ui-test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)

add_executable(mpris-player-discovery-test
    test_mprisplayerdiscovery.cpp
)

target_link_libraries(mpris-player-discovery-test
    PRIVATE
        Lyrics::Mpris
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(
    NAME mpris-player-discovery-test
    COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:mpris-player-discovery-test>
)
set_tests_properties(mpris-player-discovery-test PROPERTIES TIMEOUT 20)

add_executable(log-engine-test
    test_logengine.cpp
)

target_link_libraries(log-engine-test
    PRIVATE
        Lyrics::Logging
        Qt6::Core
        Qt6::Test
)

add_test(NAME log-engine-test COMMAND log-engine-test)

add_executable(lyrics-service-controller-test
    test_lyricsservicecontroller.cpp
)

target_link_libraries(lyrics-service-controller-test
    PRIVATE
        Lyrics::Service
        Qt6::Core
        Qt6::Test
)

add_test(NAME lyrics-service-controller-test COMMAND lyrics-service-controller-test)

add_executable(opencc-chinese-script-converter-test
    test_openccchinesescriptconverter.cpp
)

target_link_libraries(opencc-chinese-script-converter-test
    PRIVATE
        Lyrics::Service
        Qt6::Core
        Qt6::Test
)

add_test(NAME opencc-chinese-script-converter-test COMMAND opencc-chinese-script-converter-test)

add_executable(lyrics-dbus-adapter-test
    test_lyricsdbusadapter.cpp
)

target_link_libraries(lyrics-dbus-adapter-test
    PRIVATE
        Lyrics::Service
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(
    NAME lyrics-dbus-adapter-test
    COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:lyrics-dbus-adapter-test>
)
set_tests_properties(lyrics-dbus-adapter-test PROPERTIES TIMEOUT 20)

add_executable(lrclib-provider-test
    test_lrclibprovider.cpp
)

target_link_libraries(lrclib-provider-test
    PRIVATE
        Lyrics::Service
        Qt6::Core
        Qt6::Network
        Qt6::Test
)

add_test(NAME lrclib-provider-test COMMAND lrclib-provider-test)
set_tests_properties(lrclib-provider-test PROPERTIES TIMEOUT 20)

add_executable(lrclib-lyrics-adapter-test
    test_lrcliblyricsadapter.cpp
)

target_link_libraries(lrclib-lyrics-adapter-test
    PRIVATE
        Lyrics::Service
        Qt6::Core
        Qt6::Sql
        Qt6::Test
)

add_test(NAME lrclib-lyrics-adapter-test COMMAND lrclib-lyrics-adapter-test)
set_tests_properties(lrclib-lyrics-adapter-test PROPERTIES TIMEOUT 20)

add_executable(lyrics-dock-viewmodel-test
    test_lyricsdockviewmodel.cpp
)

target_link_libraries(lyrics-dock-viewmodel-test
    PRIVATE
        Lyrics::DockViewModel
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(
    NAME lyrics-dock-viewmodel-test
    COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:lyrics-dock-viewmodel-test>
)
set_tests_properties(lyrics-dock-viewmodel-test PROPERTIES TIMEOUT 20)

add_executable(lyrics-dock-plugin-test
    test_lyricsdockplugin.cpp
)

target_compile_definitions(lyrics-dock-plugin-test PRIVATE
    LYRICS_DOCK_PLUGIN_PATH="$<TARGET_FILE:ds-lyrics-dock>"
    LYRICS_DOCK_PACKAGE_PATH="${CMAKE_BINARY_DIR}/packages/org.deepin.ds.lyrics-dock"
)

target_link_libraries(lyrics-dock-plugin-test
    PRIVATE
        Dde::Shell
        Qt6::Core
        Qt6::Test
)

add_dependencies(lyrics-dock-plugin-test
    ds-lyrics-dock
    org.deepin.ds.lyrics-dock_package
)

add_test(NAME lyrics-dock-plugin-test COMMAND lyrics-dock-plugin-test)

add_executable(lyric-bar-test
    test_lyricbar.cpp
)

target_compile_definitions(lyric-bar-test PRIVATE
    LYRICS_DOCK_PACKAGE_PATH="${CMAKE_BINARY_DIR}/packages/org.deepin.ds.lyrics-dock"
)

target_link_libraries(lyric-bar-test
    PRIVATE
        Lyrics::Ui
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
        Dtk6::Gui
)

add_dependencies(lyric-bar-test org.deepin.ds.lyrics-dock_package)
add_test(NAME lyric-bar-test COMMAND lyric-bar-test)
set_tests_properties(lyric-bar-test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(settings-viewmodel-test
    test_settingsviewmodel.cpp
)

target_link_libraries(settings-viewmodel-test
    PRIVATE
        Lyrics::SettingsViewModel
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(
    NAME settings-viewmodel-test
    COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:settings-viewmodel-test>
)
set_tests_properties(settings-viewmodel-test PROPERTIES TIMEOUT 20)

add_executable(settings-window-test
    test_settingswindow.cpp
)

target_link_libraries(settings-window-test
    PRIVATE
        Lyrics::SettingsViewModel
        Lyrics::SettingsWindow
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        Qt6::Widgets
        Dtk6::Widget
)

add_test(
    NAME settings-window-test
    COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:settings-window-test>
)
set_tests_properties(settings-window-test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 20
)
