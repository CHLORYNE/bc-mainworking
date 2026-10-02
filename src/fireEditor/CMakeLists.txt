
add_executable(bridgecommand-fe
    main.cpp
    FireEditor.cpp
    FireMenu.cpp
    FireScenario.cpp
    ../chartView/ChartView.cpp
    ../chartView/ChartDraw.cpp
    ../IncidentConfig.cpp
    ../IniFile.cpp
    ../Utilities.cpp
    ../ScenarioDataStructure.cpp
)

target_link_libraries(bridgecommand-fe
    bc-irrlicht
)
