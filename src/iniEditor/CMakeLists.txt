
add_executable(bridgecommand-ini
    main.cpp
    ../AdminLock.cpp
    ../IniFile.cpp
    ../Lang.cpp
    ../Utilities.cpp
)

target_link_libraries(bridgecommand-ini
    bc-irrlicht
)
