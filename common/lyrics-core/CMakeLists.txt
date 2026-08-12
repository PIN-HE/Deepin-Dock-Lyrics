add_library(lyrics-core STATIC
    src/lrcparser.cpp
    src/lyricsync.cpp
    src/normalization.cpp
)

add_library(Lyrics::Core ALIAS lyrics-core)

target_include_directories(lyrics-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(lyrics-core
    PUBLIC
        Qt6::Core
)

target_compile_features(lyrics-core PUBLIC cxx_std_17)
