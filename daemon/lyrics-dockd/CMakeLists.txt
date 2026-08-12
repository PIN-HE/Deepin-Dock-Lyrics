add_executable(lyrics-dockd
    main.cpp
)

target_link_libraries(lyrics-dockd
    PRIVATE
        Lyrics::Core
        Qt6::Core
        Qt6::DBus
        Qt6::Network
        Qt6::Sql
)
