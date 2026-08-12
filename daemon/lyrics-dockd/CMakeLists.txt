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

add_library(lyrics-service STATIC
    application/lyricsservicecontroller.cpp
    application/lyricsservicecontroller.h
    infrastructure/dconfigsettingsadapter.cpp
    infrastructure/dconfigsettingsadapter.h
    infrastructure/audiospectrumanalyzer.cpp
    infrastructure/audiospectrumanalyzer.h
    infrastructure/lyricsdbusadapter.cpp
    infrastructure/lyricsdbusadapter.h
    infrastructure/lrcliblyricsadapter.cpp
    infrastructure/lrcliblyricsadapter.h
    infrastructure/lrclibprovider.cpp
    infrastructure/lrclibprovider.h
    infrastructure/mprisplayeradapter.cpp
    infrastructure/mprisplayeradapter.h
    infrastructure/openccchinesescriptconverter.cpp
    infrastructure/openccchinesescriptconverter.h
    infrastructure/pipewirestreammatcher.cpp
    infrastructure/pipewirestreammatcher.h
    infrastructure/pipewireaudiovisualizeradapter.cpp
    infrastructure/pipewireaudiovisualizeradapter.h
    infrastructure/qtnetworktransport.cpp
    infrastructure/qtnetworktransport.h
    infrastructure/sqlitelyricscache.cpp
    infrastructure/sqlitelyricscache.h
    ports/httptransport.h
    ports/audiovisualizerport.h
    ports/chinesescriptconverter.h
    ports/lyricscache.h
    ports/lyricsport.h
    ports/playerport.h
    ports/settingsport.h
)

if(PipeWire_FOUND)
    target_compile_definitions(lyrics-service PUBLIC DEEPIN_DOCK_LYRICS_HAS_PIPEWIRE=1)
    target_link_libraries(lyrics-service PUBLIC PkgConfig::PipeWire)
endif()

add_library(Lyrics::Service ALIAS lyrics-service)

target_include_directories(lyrics-service
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(lyrics-service
    PUBLIC
        Lyrics::Core
        Lyrics::Logging
        Lyrics::Mpris
        Lyrics::Version
        Qt6::Core
        Qt6::DBus
        Qt6::Network
        Qt6::Sql
        Dtk6::Core
        PkgConfig::OpenCC
)

add_executable(lyrics-dockd
    main.cpp
)

target_link_libraries(lyrics-dockd
    PRIVATE
        Lyrics::Service
        Lyrics::Version
        Qt6::Core
        Qt6::DBus
        Qt6::Network
        Qt6::Sql
        Dtk6::Core
)

dtk_add_config_meta_files(
    APPID org.deepin.LyricsDock
    FILES ${CMAKE_SOURCE_DIR}/config/org.deepin.LyricsDock.json
)
