
add_executable(bridgecommand-ed
    main.cpp
    ../IniFile.cpp
    ../Lang.cpp
    ../Utilities.cpp
    ../ScenarioDataStructure.cpp
    ../IncidentConfig.cpp
    ../chartView/ChartView.cpp
    ../chartView/ChartDraw.cpp
    ControllerModel.cpp
    EventReceiver.cpp
    ImportExportGUI.cpp
    GUI.cpp
    StartupEventReceiver.cpp
)


target_link_libraries(bridgecommand-ed
    bc-irrlicht
)
