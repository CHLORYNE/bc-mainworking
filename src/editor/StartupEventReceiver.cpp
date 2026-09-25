/* Bridge Command 5.0 Ship Simulator ... */

#include "StartupEventReceiver.hpp"
#include <iostream>
#include <fstream> 
#include <string>
#include <cstdio>
#include <cstdlib>
#include "../ScenarioDataStructure.hpp"
#include "../Utilities.hpp"
#include "ImportExportGUI.hpp"
#include "../IniFile.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Shellapi.h>
#else
#include <stdlib.h>
#endif
#include <algorithm>

StartupEventReceiver::StartupEventReceiver(
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
    irr::video::IVideoDriver* driver)
{
    this->scenarioListBox = scenarioListBox;
    this->worldListBox = worldListBox;
    this->selectWindow = selectWindow;
    this->scenarioListBoxID = scenarioListBoxID;
    this->worldListBoxID = worldListBoxID;
    this->okScenarioButtonID = okScenarioButtonID;
    this->okWorldButtonID = okWorldButtonID;
    this->importScenarioButtonID = importScenarioButtonID;
    this->exportScenarioButtonID = exportScenarioButtonID;
    this->importExportOKButtonID = importExportOKButtonID;
    this->deleteScenarioButtonID = deleteScenarioButtonID;
    this->guiImportExport = guiImportExport;
    this->scenarioData = scenarioData;
    this->metadataPanel = metadataPanel;
    this->mapThumbText = mapThumbText;
    this->driver = driver;
    scenarioSelected = -1;
    worldSelected = -1;
}

// --- BULLETPROOF DIRECT INI READERS & UTF-8 CONVERTER ---
namespace {

    // Safely converts UTF-8 text (like French accents) into wide strings for Irrlicht
    std::wstring utf8_to_wstring(const std::string& str) {
        std::wstring wstr;
        for (size_t i = 0; i < str.length(); ) {
            unsigned char c = str[i];
            if (c < 0x80) {
                wstr += (wchar_t)c;
                i++;
            }
            else if ((c >> 5) == 0x06) {
                if (i + 1 < str.length()) {
                    wstr += (wchar_t)(((c & 0x1f) << 6) | (str[i + 1] & 0x3f));
                }
                i += 2;
            }
            else if ((c >> 4) == 0x0E) {
                if (i + 2 < str.length()) {
                    wstr += (wchar_t)(((c & 0x0f) << 12) | ((str[i + 1] & 0x3f) << 6) | (str[i + 2] & 0x3f));
                }
                i += 3;
            }
            else if ((c >> 3) == 0x1E) {
                i += 4; // Skip 4-byte sequences for standard wchar_t safety
            }
            else { i++; }
        }
        return wstr;
    }

    std::string DirectIniReadStr(const std::string& filename, const std::string& key) {
        std::ifstream file(filename.c_str());
        if (!file.is_open()) return "";
        std::string line;

        std::string searchKey = key;
        std::transform(searchKey.begin(), searchKey.end(), searchKey.begin(), ::tolower);

        while (std::getline(file, line)) {
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string k = line.substr(0, eqPos);

                size_t start = k.find_first_not_of(" \t\r\n");
                if (start != std::string::npos) {
                    k.erase(0, start);
                    k.erase(k.find_last_not_of(" \t\r\n") + 1);
                }
                else {
                    continue;
                }

                std::transform(k.begin(), k.end(), k.begin(), ::tolower);

                if (k == searchKey) {
                    std::string v = line.substr(eqPos + 1);

                    size_t vStart = v.find_first_not_of(" \t\r\n\"");
                    if (vStart != std::string::npos) {
                        v = v.substr(vStart);
                        v.erase(v.find_last_not_of(" \t\r\n\"") + 1);
                        v.erase(std::remove(v.begin(), v.end(), '\r'), v.end());
                        return v;
                    }
                    else {
                        return "";
                    }
                }
            }
        }
        return "";
    }

    float DirectIniReadFloat(const std::string& filename, const std::string& key) {
        std::string v = DirectIniReadStr(filename, key);
        return v.empty() ? 0.0f : (float)atof(v.c_str());
    }

    int DirectIniReadInt(const std::string& filename, const std::string& key) {
        std::string v = DirectIniReadStr(filename, key);
        return v.empty() ? 0 : atoi(v.c_str());
    }
}

// Extract logic to populate map details
void StartupEventReceiver::updateMapMetadata() {
    irr::s32 selectedIdx = worldListBox->getSelected();
    if (selectedIdx > -1) {
        std::wstring worldWName = std::wstring(worldListBox->getListItem(selectedIdx));
        std::string worldName(worldWName.begin(), worldWName.end());

        std::string descPathBase = "World/" + worldName + "/description.txt";
        std::string userFolder = Utilities::getUserDir();
        std::string descPathUser = userFolder + descPathBase;

        std::ifstream descFile;
        if (Utilities::pathExists(descPathUser)) {
            descFile.open(descPathUser.c_str());
        }
        else {
            descFile.open(descPathBase.c_str());
        }

        mapThumbText->clear();
        irr::core::stringw title = L"Carte: "; title += utf8_to_wstring(worldName).c_str();
        mapThumbText->addItem(title.c_str());
        mapThumbText->addItem(L"------------------");

        if (descFile.is_open()) {
            std::string line;
            while (std::getline(descFile, line)) {
                line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
                // Pass text through UTF-8 converter to fix French Accents
                mapThumbText->addItem(irr::core::stringw(utf8_to_wstring(line).c_str()).c_str());
            }
            descFile.close();
        }
        else {
            mapThumbText->addItem(L"Aucune description trouvée.");
        }
    }
}

// Extract logic to populate scenario details
void StartupEventReceiver::updateScenarioMetadata() {
    irr::s32 selectedIdx = scenarioListBox->getSelected();
    if (selectedIdx > -1) {
        std::wstring scenarioWName = std::wstring(scenarioListBox->getListItem(selectedIdx));
        std::string scenarioName(scenarioWName.begin(), scenarioWName.end());

        std::string userFolder = Utilities::getUserDir();
        std::string scenarioPath = "Scenarios/";
        if (Utilities::pathExists(userFolder + scenarioPath)) { scenarioPath = userFolder + scenarioPath; }

        std::string envIni = scenarioPath + scenarioName + "/environment.ini";
        std::string otherIni = scenarioPath + scenarioName + "/othership.ini";
        std::string ownShipIni = scenarioPath + scenarioName + "/ownship.ini";

        // Read direct from disk to avoid cache issues
        float weather = DirectIniReadFloat(envIni, "Weather");
        float vis = DirectIniReadFloat(envIni, "VisibilityRange");
        float startTime = DirectIniReadFloat(envIni, "StartTime");
        float windDir = DirectIniReadFloat(envIni, "WindDirection");
        float windSpeed = DirectIniReadFloat(envIni, "WindSpeed");
        std::string mapName = DirectIniReadStr(envIni, "Setting");
// kyara fix showing CAP on scenario meta data of ownship
        std::string ownShipName = DirectIniReadStr(ownShipIni, "ShipName");
        float ownSpeed = DirectIniReadFloat(ownShipIni, "InitialSpeed");
        float ownCourse = DirectIniReadFloat(ownShipIni, "InitialBearing"); // NEW: Get Cap

        int ships = DirectIniReadInt(otherIni, "Number");

        // Format Time strictly to HH:MM
        int h = (int)startTime;
        int m = (int)((startTime - h) * 60.0f);
        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", h, m);

        // Limit all floats to 1 decimal place using snprintf buffers
        char meteoStr[16], visStr[16], ownSpdStr[16], ownCrsStr[16], windDirStr[16], windSpdStr[16];
        snprintf(meteoStr, sizeof(meteoStr), "%.1f", weather);
        snprintf(visStr, sizeof(visStr), "%.1f", vis);
        snprintf(ownSpdStr, sizeof(ownSpdStr), "%.1f", ownSpeed);
        snprintf(ownCrsStr, sizeof(ownCrsStr), "%.1f", ownCourse); // KYARA UPDATE NEW: Format Cap for OWNHSIP
        snprintf(windDirStr, sizeof(windDirStr), "%.1f", windDir);
        snprintf(windSpdStr, sizeof(windSpdStr), "%.1f", windSpeed);

        metadataPanel->clear();
        metadataPanel->addItem(L"--- Détails du Scénario ---");

        // MAP & TIME
        irr::core::stringw lineMap = L"Carte: ";
        lineMap += utf8_to_wstring(mapName).c_str();
        lineMap += L"  |  Heure: ";
        lineMap += timeStr;
        metadataPanel->addItem(lineMap.c_str());
        
        //THIS ADDS SPACE metadataPanel->addItem(L"");

        // ENVIRONMENT
        irr::core::stringw lineEnv = L"Météo: "; lineEnv += meteoStr; lineEnv += L"  |  Visibilité: "; lineEnv += visStr; lineEnv += L" Nm";
        metadataPanel->addItem(lineEnv.c_str());

        // WIND (Removed @ symbol)
        irr::core::stringw lineWind = L"Vent: "; lineWind += windSpdStr; lineWind += L" nds | Direction: "; lineWind += windDirStr; lineWind += L"°";
        metadataPanel->addItem(lineWind.c_str());
        metadataPanel->addItem(L"");

        // OWN SHIP (Added Cap)
        irr::core::stringw lineOwn = L"Votre Navire: ";
        lineOwn += utf8_to_wstring(ownShipName).c_str();
        lineOwn += L" (Cap: "; lineOwn += ownCrsStr; lineOwn += L"° | Vitesse: "; lineOwn += ownSpdStr; lineOwn += L" nds)";
        metadataPanel->addItem(lineOwn.c_str());
        metadataPanel->addItem(L"");

        // OTHER SHIPS
        irr::core::stringw lineTraf = L"Autres Navires (Trafic): "; lineTraf += irr::core::stringw(ships);
        metadataPanel->addItem(lineTraf.c_str());

        // Fetch and append other ships (Name + MMSI)
        if (ships > 0) {
            for (int i = 1; i <= ships; i++) {
                char keyType[32], keyMmsi[32], keySpd[32], keyCrs[32];
                snprintf(keyType, sizeof(keyType), "Type(%d)", i);
                snprintf(keyMmsi, sizeof(keyMmsi), "mmsi(%d)", i);
                snprintf(keySpd, sizeof(keySpd), "Speed(%d,1)", i);
                snprintf(keyCrs, sizeof(keyCrs), "Bearing(%d,1)", i);

                std::string sName = DirectIniReadStr(otherIni, keyType);
                std::string sMmsi = DirectIniReadStr(otherIni, keyMmsi);
                std::string sSpd = DirectIniReadStr(otherIni, keySpd);
                std::string sCrs = DirectIniReadStr(otherIni, keyCrs);

                if (sName.empty()) sName = "Inconnu";
                if (sMmsi.empty()) sMmsi = "0";
                if (sSpd.empty()) sSpd = "0";
                if (sCrs.empty()) sCrs = "0";

                irr::core::stringw s1 = L" - ";
                s1 += utf8_to_wstring(sName).c_str();
                s1 += L" (MMSI: "; s1 += sMmsi.c_str(); s1 += L")";
                metadataPanel->addItem(s1.c_str());

                irr::core::stringw s2 = L"      Cap: "; s2 += sCrs.c_str(); s2 += L"° | Vitesse: "; s2 += sSpd.c_str(); s2 += L" nds";
                metadataPanel->addItem(s2.c_str());
            }
        }
    }
}

// Function to call initially to load both previews
void StartupEventReceiver::forceUpdatePreview() {
    updateScenarioMetadata();
    updateMapMetadata();
}

bool StartupEventReceiver::OnEvent(const irr::SEvent& event)
{
    if (event.EventType == irr::EET_GUI_EVENT)
    {
        irr::s32 id = event.GUIEvent.Caller->getID();

        //If OK button, or double click on list, for scenario
        if ((event.GUIEvent.EventType == irr::gui::EGET_BUTTON_CLICKED && id == okScenarioButtonID) || (event.GUIEvent.EventType == irr::gui::EGET_LISTBOX_SELECTED_AGAIN && id == scenarioListBoxID))
        {
            if (scenarioListBox->getSelected() > -1) {
                scenarioSelected = scenarioListBox->getSelected();
            }
        }

        //If OK button ONLY for world
        if (event.GUIEvent.EventType == irr::gui::EGET_BUTTON_CLICKED && id == okWorldButtonID)
        {
            if (worldListBox->getSelected() > -1) {
                worldSelected = worldListBox->getSelected();
            }
        }

        // --- LIVE PREVIEW LOGIC ---
        if (event.GUIEvent.EventType == irr::gui::EGET_LISTBOX_CHANGED)
        {
            if (id == scenarioListBoxID) {
                updateScenarioMetadata();
            }
            else if (id == worldListBoxID) {
                updateMapMetadata();
            }
        }

        // Other buttons
        if (event.GUIEvent.EventType == irr::gui::EGET_BUTTON_CLICKED)
        {
            if (id == importScenarioButtonID) {
                guiImportExport->setText("");
                selectWindow->setVisible(false);
                guiImportExport->setVisible(true, 1);
            }

            if (id == exportScenarioButtonID) {
                if (scenarioListBox->getSelected() > -1) {
                    std::wstring scenarioWName = std::wstring(scenarioListBox->getListItem(scenarioListBox->getSelected()));
                    std::string scenarioName(scenarioWName.begin(), scenarioWName.end());

                    std::string userFolder = Utilities::getUserDir();
                    std::string scenarioPath = "Scenarios/";
                    if (Utilities::pathExists(userFolder + scenarioPath)) {
                        scenarioPath = userFolder + scenarioPath;
                    }
                    ScenarioData scenarioData = Utilities::getScenarioDataFromFile(scenarioPath + scenarioName, scenarioName);
                    guiImportExport->setText(scenarioData.serialise(true));
                }
                selectWindow->setVisible(false);
                guiImportExport->setVisible(true, 0);
            }

            if (id == importExportOKButtonID) {
                if (guiImportExport->getMode() == 1) {
                    scenarioData->deserialise(guiImportExport->getText());
                }
                guiImportExport->setVisible(false, 0);
                selectWindow->setVisible(true);
            }

            // --- DELETE SCENARIO LOGIC ---
            if (id == deleteScenarioButtonID) {
                irr::s32 selectedIdx = scenarioListBox->getSelected();
                if (selectedIdx > -1) {
                    std::wstring scenarioWName = std::wstring(scenarioListBox->getListItem(selectedIdx));
                    std::string scenarioName(scenarioWName.begin(), scenarioWName.end());

                    std::string userFolder = Utilities::getUserDir();
                    std::string scenarioPath = "Scenarios/";
                    if (Utilities::pathExists(userFolder + scenarioPath)) {
                        scenarioPath = userFolder + scenarioPath;
                    }
                    std::string fullPath = scenarioPath + scenarioName;

#ifdef _WIN32
                    std::string dirPath = fullPath;
                    dirPath.append(1, '\0');
                    std::replace(dirPath.begin(), dirPath.end(), '/', '\\');
                    SHFILEOPSTRUCT fileOp;
                    memset(&fileOp, 0, sizeof(fileOp));
                    fileOp.wFunc = FO_DELETE;
                    fileOp.pFrom = dirPath.c_str();
                    fileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
                    SHFileOperation(&fileOp);
#else
                    std::string command = "rm -rf \"" + fullPath + "\"";
                    system(command.c_str());
#endif

                    scenarioListBox->removeItem(selectedIdx);
                    // --- CLEARS THE PANEL AFTER DELETION ---
                    metadataPanel->clear();
                    metadataPanel->addItem(L"Sélectionnez un exercice pour voir les détails...");
                }
            }
        }
    }
    return false;
}

irr::s32 StartupEventReceiver::getScenarioSelected() const
{
    return scenarioSelected;
}

irr::s32 StartupEventReceiver::getWorldSelected() const
{
    return worldSelected;
}
