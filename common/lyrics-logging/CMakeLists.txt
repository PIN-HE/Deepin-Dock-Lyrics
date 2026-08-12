add_library(lyrics-logging STATIC
    src/logengine.cpp
    include/lyricslogging/logengine.h
)

add_library(Lyrics::Logging ALIAS lyrics-logging)

target_include_directories(lyrics-logging
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(lyrics-logging
    PUBLIC
        Qt6::Core
)
