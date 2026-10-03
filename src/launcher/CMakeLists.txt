
add_executable(bridgecommand
    main.cpp
    HudMenu.cpp
    HudText.cpp
    UiSound.cpp
    VideoClip.cpp
    ../IniFile.cpp
    ../Lang.cpp
    ../Utilities.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(bridgecommand
    bc-irrlicht
    Threads::Threads
)
if(WIN32)
    #waveOut (menu sounds, film sound) and COM (Media Foundation, loaded at run time)
    target_link_libraries(bridgecommand winmm ole32)
endif()
