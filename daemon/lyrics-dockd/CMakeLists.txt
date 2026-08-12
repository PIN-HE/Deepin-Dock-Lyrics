add_library(lyrics-mpris STATIC
    mprisplayerdiscovery.cpp
    mprisplayerdiscovery.h
)

add_library(Lyrics::Mpris ALIAS lyrics-mpris)

target_include_directories(lyrics-mpris
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(lyrics-mpris
    PUBLIC
        Lyrics::Core
        Qt6::Core
        Qt6::DBus
)

add_executable(lyrics-dockd
    main.cpp
)

target_link_libraries(lyrics-dockd
    PRIVATE
        Lyrics::Mpris
        Qt6::Core
        Qt6::DBus
        Qt6::Network
        Qt6::Sql
)
