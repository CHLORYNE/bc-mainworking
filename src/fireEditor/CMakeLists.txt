
add_executable(bridgecommand-fe
    main.cpp
    FireEditor.cpp
    FireMap.cpp
    FireScenario.cpp
    ../IncidentConfig.cpp
    ../IniFile.cpp
    ../Utilities.cpp
    ../ScenarioDataStructure.cpp
)

target_link_libraries(bridgecommand-fe
    bc-irrlicht
)
