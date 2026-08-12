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
