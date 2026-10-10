
add_executable(bridgecommand-mh
    main.cpp
    ../IniFile.cpp
    ../Lang.cpp
    ../Utilities.cpp
    ../ScenarioDataStructure.cpp
    LinesData.cpp
    Network.cpp
    ScenarioChoice.cpp
    ShipPositions.cpp
    StartupEventReceiver.cpp
    EventReceiver.cpp
    InstructorStation.cpp
    ../chartView/ChartView.cpp
    ../chartView/ChartDraw.cpp
    ../IncidentConfig.cpp
)

target_link_libraries(bridgecommand-mh
    enet
    bc-irrlicht
)
