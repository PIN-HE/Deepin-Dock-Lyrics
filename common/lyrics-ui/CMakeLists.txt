add_library(lyrics-ui STATIC
    include/lyricsui/lyricstokens.h
    include/lyricsui/qmlregistration.h
    src/lyricstokens.cpp
    src/qmlregistration.cpp
)

add_library(Lyrics::Ui ALIAS lyrics-ui)

target_include_directories(lyrics-ui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(lyrics-ui
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Dtk6::Gui
)

target_compile_features(lyrics-ui PUBLIC cxx_std_17)
