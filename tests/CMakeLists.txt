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
