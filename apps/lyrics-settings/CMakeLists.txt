add_executable(deepin-lyrics-settings
    main.cpp
)

target_link_libraries(deepin-lyrics-settings
    PRIVATE
        Lyrics::Core
        Qt6::Core
        Qt6::Widgets
        Dtk6::Core
        Dtk6::Gui
        Dtk6::Widget
)
