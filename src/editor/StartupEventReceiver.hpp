/* Bridge Command 5.0 Ship Simulator ... */

#ifndef __STARTUPEVENTRECEIVER_HPP_INCLUDED__
#define __STARTUPEVENTRECEIVER_HPP_INCLUDED__

#include "irrlicht.h"

//Forward declarations
class GUIImportExport;
class ScenarioData;

class StartupEventReceiver : public irr::IEventReceiver
{
public:

    StartupEventReceiver(
        irr::gui::IGUIListBox* scenarioListBox,
        irr::gui::IGUIListBox* worldListBox,
        irr::gui::IGUIWindow* selectWindow,
        irr::s32 scenarioListBoxID,
        irr::s32 worldListBoxID,
        irr::s32 okScenarioButtonID,
        irr::s32 okWorldButtonID,
        irr::s32 importScenarioButtonID,
        irr::s32 exportScenarioButtonID,
        irr::s32 importExportOKButtonID,
        irr::s32 deleteScenarioButtonID,
        GUIImportExport* guiImportExport,
        ScenarioData* scenarioData,
        irr::gui::IGUIListBox* metadataPanel, // STRICTLY ListBox
        irr::gui::IGUIListBox* mapThumbText,  // STRICTLY ListBox
        irr::video::IVideoDriver* driver);

    bool OnEvent(const irr::SEvent& event);

    irr::s32 getScenarioSelected() const;
    irr::s32 getWorldSelected() const;

    void forceUpdatePreview();
    void updateScenarioMetadata();
    void updateMapMetadata();

private:

    irr::gui::IGUIListBox* scenarioListBox;
    irr::gui::IGUIListBox* worldListBox;
    irr::gui::IGUIWindow* selectWindow;
    GUIImportExport* guiImportExport;
    ScenarioData* scenarioData;

    irr::s32 scenarioListBoxID;
    irr::s32 worldListBoxID;
    irr::s32 okScenarioButtonID;
    irr::s32 okWorldButtonID;
    irr::s32 importScenarioButtonID;
    irr::s32 exportScenarioButtonID;
    irr::s32 importExportOKButtonID;
    irr::s32 deleteScenarioButtonID;
    irr::s32 scenarioSelected;
    irr::s32 worldSelected;

    irr::gui::IGUIListBox* metadataPanel; // STRICTLY ListBox
    irr::gui::IGUIListBox* mapThumbText;  // STRICTLY ListBox
    irr::video::IVideoDriver* driver;

};

#endif