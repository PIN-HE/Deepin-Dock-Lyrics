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
