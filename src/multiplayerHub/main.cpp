/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2016 James Packer

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY Or FITNESS For A PARTICULAR PURPOSE.  See the
     GNU General Public License For more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

// main.cpp

#include <iostream>
#include <chrono>

// Include the Irrlicht header
#include "irrlicht.h"
#include "../Utilities.hpp"
#include "../Constants.hpp"
#include "../IniFile.hpp"
#include "../ScenarioDataStructure.hpp"
#include "../Lang.hpp"
#include "ScenarioChoice.hpp"
#include "Network.hpp"
#include "ShipPositions.hpp"
#include "LinesData.hpp"
#include "EventReceiver.hpp"
#include "../UiTheme.hpp"

#include <fstream> //To save to log

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // For GetSystemMetrics
#include <direct.h> //for windows _mkdir
#else
#include <sys/stat.h>
#endif // _WIN32


//Mac OS:
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef _MSC_VER
#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

//Set up global for ini reader to have access to irrlicht logger if needed.
namespace IniFile {
    irr::ILogger* irrlichtLogger = 0;
}

std::string makeTimeString(uint64_t absoluteTime, uint64_t offsetTime, irr::f32 scenarioTime, irr::f32 accelerator)
{
    //timestamp (unix),
    //timestamp of start of first scenario day,
    //time since start of first scenario day (float),
    //accelerator#
    std::string timeString = Utilities::lexical_cast<std::string>(absoluteTime);
    timeString.append(",");
    timeString.append(Utilities::lexical_cast<std::string>(offsetTime));
    timeString.append(",");
    timeString.append(Utilities::lexical_cast<std::string>(scenarioTime));
    timeString.append(",");
    timeString.append(Utilities::lexical_cast<std::string>(accelerator));
    return timeString;
}

//Live view while the exercise runs: the exercise clock and state, and each station's ship.
class HubDashboard : public irr::gui::IGUIElement
{
public:
    struct Station {
        std::wstring host, ship;
        irr::f32 speedKts, heading;
    };

    HubDashboard(irr::gui::IGUIEnvironment* env, bool french, const std::wstring& exercise, irr::gui::IGUIFont* bigFont,
        irr::gui::IGUIFont* titleFont, irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* smallFont)
        : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, env, env->getRootGUIElement(), -1,
            irr::core::rect<irr::s32>(irr::core::position2di(0, 0), env->getVideoDriver()->getScreenSize())),
        french(french), exercise(exercise), bigFont(bigFont), titleFont(titleFont), textFont(textFont), smallFont(smallFont),
        running(false), accelerator(0), lines(0)
    {
        const irr::f32 W = (irr::f32)AbsoluteRect.getWidth(), H = (irr::f32)AbsoluteRect.getHeight();
        const irr::f32 split = irr::core::clamp(W * 0.34f, 300.0f, 420.0f);
        clockCard = irr::core::rect<irr::f32>(28, 112, split, H - 64);
        stationCard = irr::core::rect<irr::f32>(split + 20, 112, W - 28, H - 64);
        const irr::f32 bx0 = clockCard.UpperLeftCorner.X + 22, bx1 = clockCard.LowerRightCorner.X - 22;
        const irr::f32 bh = 46;
        run = irr::core::rect<irr::f32>(bx0, clockCard.LowerRightCorner.Y - 22 - 2 * bh - 12, bx1, clockCard.LowerRightCorner.Y - 22 - bh - 12);
        pause = irr::core::rect<irr::f32>(bx0, clockCard.LowerRightCorner.Y - 22 - bh, bx1, clockCard.LowerRightCorner.Y - 22);
    }

    void setUnreached(const std::vector<std::wstring>& names) { unreached = names; }

    irr::core::rect<irr::s32> runRect() const { return Ui::toI(run); }
    irr::core::rect<irr::s32> pauseRect() const { return Ui::toI(pause); }

    void update(const std::wstring& clockText, const std::wstring& dateText, irr::f32 acceleratorNow, const std::vector<Station>& stationsNow, irr::u32 linesNow)
    {
        clock = clockText;
        date = dateText;
        accelerator = acceleratorNow;
        running = acceleratorNow > 0;
        stations = stationsNow;
        lines = linesNow;
    }

    virtual void draw()
    {
        if (!IsVisible) { return; }
        irr::video::IVideoDriver* driver = Environment->getVideoDriver();
        const irr::f32 W = (irr::f32)AbsoluteRect.getWidth(), H = (irr::f32)AbsoluteRect.getHeight();
        driver->draw2DRectangle(irr::core::rect<irr::s32>(0, 0, (irr::s32)W, (irr::s32)H), irr::video::SColor(255, 16, 32, 58),
            irr::video::SColor(255, 16, 32, 58), Ui::backgroundDeep, Ui::backgroundDeep);

        //State pill, at the right of the header.
        const std::wstring state = running ? (french ? L"EN COURS" : L"RUNNING") : (french ? L"EN PAUSE" : L"PAUSED");
        const irr::video::SColor stateCol = running ? Ui::success : Ui::warning;
        const irr::f32 pw = Ui::textWidth(smallFont, state) + 44;
        const irr::core::rect<irr::f32> pill(W - 28 - pw, 34, W - 28, 66);

        const irr::f32 rowH = Ui::textHeight(textFont) + 18;
        const irr::f32 colStation = stationCard.UpperLeftCorner.X + 24;
        const irr::f32 colShip = stationCard.UpperLeftCorner.X + stationCard.getWidth() * 0.36f;
        const irr::f32 colSpeed = stationCard.UpperLeftCorner.X + stationCard.getWidth() * 0.70f;
        const irr::f32 colHeading = stationCard.LowerRightCorner.X - 24;
        const irr::f32 tableTop = stationCard.UpperLeftCorner.Y + 62;

        irr::gui::PanelBatch b;
        b.begin(driver);
        b.disc(irr::core::vector2df(56, 56), 26, irr::video::SColor(70, 64, 156, 240), irr::video::SColor(70, 64, 156, 240));
        Ui::networkIcon(b, irr::core::vector2df(56, 56), 14, Ui::accentHi);
        b.rect(irr::core::rect<irr::f32>(28, 96, W - 28, 97), Ui::rule);
        Ui::roundRect(b, pill, 16, Ui::alpha(stateCol, 40), Ui::alpha(stateCol, 40));
        Ui::roundRectOutline(b, pill, 16, 1.0f, Ui::alpha(stateCol, 160));
        b.disc(irr::core::vector2df(pill.UpperLeftCorner.X + 18, pill.getCenter().Y), 5, stateCol, stateCol);
        Ui::card(b, clockCard, 14);
        Ui::card(b, stationCard, 14);
        //Table: header rule and alternate row bands.
        b.rect(irr::core::rect<irr::f32>(stationCard.UpperLeftCorner.X + 20, tableTop + rowH - 6, stationCard.LowerRightCorner.X - 20, tableTop + rowH - 5), Ui::rule);
        for (size_t i = 0; i < stations.size(); i++) {
            const irr::f32 y = tableTop + rowH * (i + 1);
            if (y + rowH > stationCard.LowerRightCorner.Y - 16) { break; }
            if (i % 2 == 0) {
                Ui::roundRect(b, irr::core::rect<irr::f32>(stationCard.UpperLeftCorner.X + 16, y, stationCard.LowerRightCorner.X - 16, y + rowH - 4), 8,
                    irr::video::SColor(255, 16, 30, 52), irr::video::SColor(255, 14, 27, 47));
            }
        }
        b.flush();

        Ui::drawText(titleFont, french ? L"Exercice en cours" : L"Exercise in progress", irr::core::rect<irr::f32>(96, 26, pill.UpperLeftCorner.X - 20, 56), Ui::text);
        Ui::drawText(smallFont, exercise, irr::core::rect<irr::f32>(96, 58, pill.UpperLeftCorner.X - 20, 80), Ui::textDim);
        Ui::drawText(smallFont, state, irr::core::rect<irr::f32>(pill.UpperLeftCorner.X + 30, pill.UpperLeftCorner.Y, pill.LowerRightCorner.X - 12, pill.LowerRightCorner.Y), stateCol);

        //Clock card
        const irr::f32 cx0 = clockCard.UpperLeftCorner.X + 24, cx1 = clockCard.LowerRightCorner.X - 24;
        Ui::drawText(textFont, french ? L"Heure de l'exercice" : L"Exercise time", irr::core::rect<irr::f32>(cx0, clockCard.UpperLeftCorner.Y + 16, cx1, clockCard.UpperLeftCorner.Y + 44), Ui::textDim);
        Ui::drawText(bigFont, clock, irr::core::rect<irr::f32>(cx0, clockCard.UpperLeftCorner.Y + 52, cx1, clockCard.UpperLeftCorner.Y + 110), Ui::text);
        Ui::drawText(textFont, date, irr::core::rect<irr::f32>(cx0, clockCard.UpperLeftCorner.Y + 112, cx1, clockCard.UpperLeftCorner.Y + 138), Ui::textDim);
        wchar_t speed[64];
        swprintf(speed, 64, french ? L"Acc\u00E9l\u00E9ration : x%g" : L"Time acceleration: x%g", accelerator);
        Ui::drawText(smallFont, speed, irr::core::rect<irr::f32>(cx0, clockCard.UpperLeftCorner.Y + 148, cx1, clockCard.UpperLeftCorner.Y + 170), Ui::textFaint);

        //Stations card
        Ui::drawText(textFont, french ? L"Postes connect\u00E9s" : L"Connected stations", irr::core::rect<irr::f32>(colStation, stationCard.UpperLeftCorner.Y + 16, colHeading, stationCard.UpperLeftCorner.Y + 44), Ui::text);
        Ui::drawText(smallFont, std::to_wstring(stations.size()), irr::core::rect<irr::f32>(colStation, stationCard.UpperLeftCorner.Y + 16, colHeading, stationCard.UpperLeftCorner.Y + 44), Ui::textDim, Ui::Right);
        const irr::core::rect<irr::f32> head(0, tableTop, 0, tableTop + rowH - 8);
        Ui::drawText(smallFont, french ? L"POSTE" : L"STATION", irr::core::rect<irr::f32>(colStation, head.UpperLeftCorner.Y, colShip - 8, head.LowerRightCorner.Y), Ui::textFaint);
        Ui::drawText(smallFont, french ? L"NAVIRE" : L"SHIP", irr::core::rect<irr::f32>(colShip, head.UpperLeftCorner.Y, colSpeed - 8, head.LowerRightCorner.Y), Ui::textFaint);
        Ui::drawText(smallFont, french ? L"VITESSE" : L"SPEED", irr::core::rect<irr::f32>(colSpeed, head.UpperLeftCorner.Y, colSpeed + 120, head.LowerRightCorner.Y), Ui::textFaint);
        Ui::drawText(smallFont, french ? L"CAP" : L"HEADING", irr::core::rect<irr::f32>(colSpeed, head.UpperLeftCorner.Y, colHeading, head.LowerRightCorner.Y), Ui::textFaint, Ui::Right);
        for (size_t i = 0; i < stations.size(); i++) {
            const irr::f32 y = tableTop + rowH * (i + 1);
            if (y + rowH > stationCard.LowerRightCorner.Y - 16) { break; }
            const irr::core::rect<irr::f32> row(0, y, 0, y + rowH - 4);
            const irr::core::rect<irr::s32> shipClip = Ui::toI(irr::core::rect<irr::f32>(colShip, y, colSpeed - 12, y + rowH));
            const irr::core::rect<irr::s32> stationClip = Ui::toI(irr::core::rect<irr::f32>(colStation, y, colShip - 12, y + rowH));
            const std::wstring who = std::to_wstring(i + 1) + L"  " + stations[i].host;
            Ui::drawText(textFont, who, irr::core::rect<irr::f32>(colStation, row.UpperLeftCorner.Y, colShip - 12, row.LowerRightCorner.Y), Ui::text, Ui::Left, &stationClip);
            Ui::drawText(textFont, stations[i].ship, irr::core::rect<irr::f32>(colShip, row.UpperLeftCorner.Y, colSpeed - 12, row.LowerRightCorner.Y), Ui::accentHi, Ui::Left, &shipClip);
            wchar_t v[32], h[32];
            swprintf(v, 32, L"%.1f %ls", stations[i].speedKts, french ? L"nds" : L"kn");
            swprintf(h, 32, L"%03.0f\u00B0", stations[i].heading);
            Ui::drawText(textFont, v, irr::core::rect<irr::f32>(colSpeed, row.UpperLeftCorner.Y, colSpeed + 140, row.LowerRightCorner.Y), Ui::text);
            Ui::drawText(textFont, h, irr::core::rect<irr::f32>(colSpeed, row.UpperLeftCorner.Y, colHeading, row.LowerRightCorner.Y), Ui::text, Ui::Right);
        }

        //Stations typed in but not answering when the exercise started.
        if (!unreached.empty()) {
            std::wstring names;
            for (size_t i = 0; i < unreached.size(); i++) { names += (i ? L", " : L"") + unreached[i]; }
            const irr::f32 y = stationCard.LowerRightCorner.Y - 46;
            Ui::drawText(smallFont, (french ? L"Postes non joints (simulateur non d\u00E9marr\u00E9 ou nom inconnu) : " : L"Stations not reached (simulator not started or unknown name): ") + names,
                irr::core::rect<irr::f32>(colStation, y, colHeading, y + 26), irr::video::SColor(255, 255, 128, 128));
        }
        if (stations.empty()) {
            Ui::drawText(textFont, french ? L"Aucun poste connect\u00E9." : L"No station connected.",
                irr::core::rect<irr::f32>(colStation, tableTop + rowH, colHeading, tableTop + 2 * rowH), Ui::textDim);
        }

        //Footer
        std::wstring footer = (french ? L"Amarres actives : " : L"Active mooring lines: ") + std::to_wstring(lines);
        footer += french ? L"     \u00B7     Clavier : 0 = pause, Entr\u00E9e = reprendre" : L"     \u00B7     Keys: 0 = pause, Enter = resume";
        Ui::drawText(smallFont, footer, irr::core::rect<irr::f32>(32, H - 50, W - 28, H - 20), Ui::textFaint);

        IGUIElement::draw(); //buttons
    }

private:
    bool french;
    std::wstring exercise;
    irr::gui::IGUIFont* bigFont;
    irr::gui::IGUIFont* titleFont;
    irr::gui::IGUIFont* textFont;
    irr::gui::IGUIFont* smallFont;
    irr::core::rect<irr::f32> clockCard, stationCard, run, pause;
    std::wstring clock, date;
    bool running;
    irr::f32 accelerator;
    std::vector<Station> stations;
    std::vector<std::wstring> unreached;
    irr::u32 lines;
};

int main()
{

    #ifdef FOR_DEB
    chdir("/usr/share/bridgecommand");
    #endif // FOR_DEB

    //Mac OS:
	#ifdef __APPLE__
    //Find starting folder
    char exePath[1024];
    uint32_t pathSize = sizeof(exePath);
    std::string exeFolderPath = "";
    if (_NSGetExecutablePath(exePath, &pathSize) == 0) {
        std::string exePathString(exePath);
        size_t pos = exePathString.find_last_of("\\/");
        if (std::string::npos != pos) {
            exeFolderPath = exePathString.substr(0, pos);
        }
    }
    //change up from BridgeCommand.app/Contents/MacOS/mh.app/Contents/MacOS to BridgeCommand.app/Contents/Resources
    exeFolderPath.append("/../../../../Resources");
    //change to this path now, so ini file is read
    chdir(exeFolderPath.c_str());
    //Note, we use this again after the createDevice call
	#endif

    //User read/write location - look in here first and the exe folder second for files
    std::string userFolder = Utilities::getUserDir();

    std::cout << "User folder is " << userFolder <<std::endl;

    /*Overview:
    Load scenario, including initial positions of each player
    Start as an enet client
    Connect to each multiplayer pc (each as a server)
    Send scenario information to each multiplayer pc (Tailored to include all other players as other ships)
    Send first update to each pc, based on initial positions
    Then loop:
        Get feedback from each pc for current position and heading
        Use this to update internal model
        Send out update to each pc, including other ship positions
    */

    //Read basic ini settings
    std::string iniFilename = "mph.ini";
    //Use local ini file if it exists
    if (Utilities::pathExists(userFolder + iniFilename)) {
        iniFilename = userFolder + iniFilename;
    }

    std::string modifier = IniFile::iniFileToString(iniFilename, "lang");
    if (modifier.length()==0) {
        modifier = "en"; //Default
    }
    std::string languageFile = "languageMultiplayer-";
    languageFile.append(modifier);
    languageFile.append(".txt");
    if (Utilities::pathExists(userFolder + languageFile)) {
        languageFile = userFolder + languageFile;
    }
    Lang language(languageFile);

    int fontSize = 12;
    float fontScale = IniFile::iniFileTof32(iniFilename, "font_scale");
    if (fontScale > 1) {
        fontSize = (int)(fontSize * fontScale + 0.5);
    } else {
	    fontScale = 1.0;
    }
    
    irr::u32 graphicsWidth = IniFile::iniFileTou32(iniFilename, "graphics_width");
    irr::u32 graphicsHeight = IniFile::iniFileTou32(iniFilename, "graphics_height");
    irr::u32 graphicsDepth = IniFile::iniFileTou32(iniFilename, "graphics_depth");
    int port = IniFile::iniFileTou32(iniFilename, "udp_send_port");

    // How long to pause between updates
    irr::u32 sleepTime = IniFile::iniFileTou32(iniFilename, "update_time");
    // Set defaults, and upper limit of 10s
    if (sleepTime==0) {
        sleepTime = 100;
    }
    if (sleepTime>10000) {
        sleepTime = 10000;
    }

    //Sensible defaults if not set
    irr::core::dimension2d<irr::u32> deskres;
    #ifdef _WIN32
    // Get the resolution (of the primary screen). Will be scaled as DPI unaware on Windows.
    deskres.Width=GetSystemMetrics(SM_CXSCREEN);
    deskres.Height=GetSystemMetrics(SM_CYSCREEN);
    #else
    // For other OSs, use Irrlicht's resolution call
    irr::IrrlichtDevice *nulldevice = irr::createDevice(irr::video::EDT_NULL);
    deskres = nulldevice->getVideoModeList()->getDesktopResolution();
    nulldevice->drop();
    #endif
    //(The desktop size can read as 0 x 0 when it cannot be found; then keep the default size.)
    if (graphicsWidth==0) {
        graphicsWidth = 1200 * fontScale;
        if (deskres.Width > 0 && graphicsWidth > deskres.Width*0.90) {
            graphicsWidth = deskres.Width*0.90;
        }
    }
    if (graphicsHeight==0) {
        graphicsHeight = 900 * fontScale;
        if (deskres.Height > 0 && graphicsHeight > deskres.Height*0.90) {
            graphicsHeight = deskres.Height*0.90;
        }
    }
    if (graphicsDepth==0) {graphicsDepth=32;}
    if (port == 0) {port = 18304;}

    //Startup irrlicht
    //create device
    irr::SIrrlichtCreationParameters deviceParameters;
    deviceParameters.DriverType = irr::video::EDT_OPENGL;
    deviceParameters.WindowSize = irr::core::dimension2d<irr::u32>(graphicsWidth,graphicsHeight);
    deviceParameters.Bits = graphicsDepth;
    irr::IrrlichtDevice* device = irr::createDeviceEx(deviceParameters);
    // CHANGES COLOR PALETTE -KYARA
    irr::gui::IGUIEnvironment* guienv = device->getGUIEnvironment();
    irr::gui::IGUISkin* skin = guienv->getSkin();

    //Dark navy look, as the launcher.
    Ui::applySkin(skin);
    const irr::s32 SCENARIO_BOX_ID = 101;
    const irr::s32 WORLD_BOX_ID = 102;
    const irr::s32 OK_SCENARIO_BUTTON_ID = 103;
    const irr::s32 OK_WORLD_BUTTON_ID = 104;
    const irr::s32 IMPORT_SCENARIO_BUTTON_ID = 105;
    const irr::s32 EXPORT_SCENARIO_BUTTON_ID = 106;
    const irr::s32 IMPORT_EXPORT_OK_BUTTON_ID = 107;
    const bool french = (modifier == "fr");
    device->setWindowCaption(french ? L"NAUTITECH - Hub multijoueur" : L"NAUTITECH - Multiplayer hub");
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::scene::ISceneManager* smgr = device->getSceneManager();

    //Chdir back on OSX
    //Mac OS:
	#ifdef __APPLE__
    chdir(exeFolderPath.c_str());
	#endif

    std::string fontName = IniFile::iniFileToString(iniFilename, "font");
    std::string fontPath = "media/fonts/" + fontName + "/" + fontName + "-" + std::to_string(fontSize) + ".xml";
    irr::gui::IGUIFont *font = device->getGUIEnvironment()->getFont(fontPath.c_str());
    if (font == NULL) {
        std::cout << "Could not load font, using fallback" << std::endl;
    } else {
        //set skin default font
        device->getGUIEnvironment()->getSkin()->setFont(font);
    }

    //Get user input for hostnames and scenario name
    std::string hostnames;
    std::string scenarioName;
    //Scenario path - default to user dir if it exists
    std::string scenarioPath = "Scenarios/";
    if (Utilities::pathExists(userFolder + scenarioPath)) {
        scenarioPath = userFolder + scenarioPath;
    }
    
    //Find default hostname if set in user directory (hostname-mp.txt)
    if (Utilities::pathExists(userFolder + "/hostname-mh.txt")) {
        hostnames=IniFile::iniFileToString(userFolder + "/hostname-mh.txt","hostname");
    }
    
    //Fonts in the sizes the screens use.
    const std::string uiFontName = fontName.empty() ? std::string("noto-sans") : fontName;
    auto uiFont = [&](int size) -> irr::gui::IGUIFont* {
        size = (int)(size * fontScale + 0.5f);
        if (size > 36) { size = 36; }
        irr::gui::IGUIFont* f = device->getGUIEnvironment()->getFont(("media/fonts/" + uiFontName + "/" + uiFontName + "-" + std::to_string(size) + ".xml").c_str());
        return f ? f : device->getGUIEnvironment()->getSkin()->getFont();
    };
    irr::gui::IGUIFont* bigFont = uiFont(36);
    irr::gui::IGUIFont* titleFont = uiFont(20);
    irr::gui::IGUIFont* textFont = uiFont(15);
    irr::gui::IGUIFont* smallFont = uiFont(13);

    ScenarioChoice scenarioChoice(device,&language,french);
    scenarioChoice.setFonts(titleFont, textFont, smallFont);
    scenarioChoice.chooseScenario(scenarioName,hostnames,scenarioPath);

    //Save hostname in user directory (hostname.txt). Check first that the location exists
    if (!Utilities::pathExists(Utilities::getUserDirBase())) {
        std::string pathToMake = Utilities::getUserDirBase();
        if (pathToMake.size() > 1) {pathToMake.erase(pathToMake.size()-1);} //Remove trailing slash
        #ifdef _WIN32
        _mkdir(pathToMake.c_str());
        #else
        mkdir(pathToMake.c_str(),0755);
        #endif // _WIN32
    }
    if (!Utilities::pathExists(Utilities::getUserDir())) {
        std::string pathToMake = Utilities::getUserDir();
        if (pathToMake.size() > 1) {pathToMake.erase(pathToMake.size()-1);} //Remove trailing slash
        #ifdef _WIN32
        _mkdir(pathToMake.c_str());
        #else
        mkdir(pathToMake.c_str(),0755);
        #endif // _WIN32
    }
    if (Utilities::pathExists(userFolder)) { 
        std::string hostnameFile = userFolder + "/hostname-mh.txt";
        std::ofstream file (hostnameFile.c_str());
        if (file.is_open()) {
            file << "hostname=" << hostnames << std::endl;
            file.close();
        }
    }


    Network network(port);
    network.connectToServer(hostnames);

    unsigned int numberOfPeers = network.getNumberOfPeers();

    std::cout << "Connected to " << numberOfPeers << " Simulateur peers." << std::endl;


    //Load overall scenario information
    ScenarioData masterScenarioData = Utilities::getScenarioDataFromFile(scenarioPath + scenarioName,scenarioName);

    irr::u32 numberOfOtherShips; // This is the number of 'other' ships in each simulation. The total number of controllable ships is numberOfOtherShips+1
    if (masterScenarioData.otherShipsData.size() > 0) {
        numberOfOtherShips = masterScenarioData.otherShipsData.size()-1;
    } else {
        numberOfOtherShips = 0;
    }

    // These both use +1 because we are storing data for all ships. numberOfOtherShips is the number of 'other' ships in each simulation, so we need to add 1 for the 'own ship'
    ShipPositions shipPositionData(numberOfOtherShips+1);
    LinesData linesData(numberOfOtherShips+1);

    //Get time information and initialise
    irr::f32 scenarioTime; //Simulation internal time, starting at zero at 0000h on start day of simulation
    uint64_t scenarioOffsetTime; //Simulation day's start time from unix epoch (1 Jan 1970)
    uint64_t absoluteTime; //Unix timestamp for current time, including start day. Calculated from scenarioTime and scenarioOffsetTime

    std::chrono::time_point<std::chrono::system_clock> currentTime = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> previousTime = currentTime;

    //irr::u32 currentTime = millisecs(); //Computer clock time (ms)
    //irr::u32 previousTime = currentTime; //Computer clock time (ms)
    irr::f32 accelerator = 1.0;

    //Add some simple information to the GUI, so the user knows it's running
    //Add text, which will list connected peers, and current time.
    std::string exerciseName = scenarioName;
    if (exerciseName.size() > 3) { exerciseName = exerciseName.substr(0, exerciseName.size() - 3); } //without _mp
    HubDashboard* dashboard = new HubDashboard(device->getGUIEnvironment(), french, std::wstring(exerciseName.begin(), exerciseName.end()),
        bigFont, titleFont, textFont, smallFont);
    std::vector<std::wstring> unreached;
    for (size_t i = 0; i < network.getUnreachedNames().size(); i++) {
        unreached.push_back(std::wstring(network.getUnreachedNames()[i].begin(), network.getUnreachedNames()[i].end()));
    }
    dashboard->setUnreached(unreached);

    // Add run and pause buttons
    irr::s32 runButtonID = 101;
    irr::s32 pauseButtonID = 102;
    Ui::Button* runButton = new Ui::Button(device->getGUIEnvironment(), dashboard, runButtonID, dashboard->runRect(),
        french ? L"D\u00E9marrer / reprendre" : L"Run / resume", Ui::Button::Primary);
    Ui::Button* pauseButton = new Ui::Button(device->getGUIEnvironment(), dashboard, pauseButtonID, dashboard->pauseRect(),
        french ? L"Pause" : L"Pause", Ui::Button::Secondary);
    runButton->setFont(textFont);
    pauseButton->setFont(textFont);
    runButton->drop();
    pauseButton->drop();

    // Setup event receiver
    EventReceiver eventReceiver(pauseButtonID, runButtonID, accelerator);
    device->setEventReceiver(&eventReceiver);

    //Fixme: Think about time zone handling
    //Fixme: Note that if the time_t isn't long enough, 2038 problem exists
    scenarioOffsetTime = Utilities::dmyToTimestamp(masterScenarioData.startDay,masterScenarioData.startMonth,masterScenarioData.startYear);//Time in seconds to start of scenario day (unix timestamp for 0000h on day scenario starts)
    scenarioTime = masterScenarioData.startTime * SECONDS_IN_HOUR; //set internal scenario time to start
    absoluteTime = Utilities::round(scenarioTime) + scenarioOffsetTime;

    //for each peer, build basic scenario information (own ship, other ships, excluding this one)
    std::vector<ScenarioData> peerScenarioData;
    for(unsigned int thisPeer = 0; thisPeer<numberOfPeers; thisPeer++ ) {
        //Own ship data gets populated from other ship (including 1st leg if it exists
        if (masterScenarioData.otherShipsData.size() > thisPeer) {

            ScenarioData thisPeerData  = masterScenarioData;

            thisPeerData.ownShipData.ownShipName = thisPeerData.otherShipsData.at(thisPeer).shipName;
            thisPeerData.ownShipData.initialLat = thisPeerData.otherShipsData.at(thisPeer).initialLat;
            thisPeerData.ownShipData.initialLong = thisPeerData.otherShipsData.at(thisPeer).initialLong;
            if (thisPeerData.otherShipsData.at(thisPeer).legs.size()>0) {
                thisPeerData.ownShipData.initialSpeed = thisPeerData.otherShipsData.at(thisPeer).legs.at(0).speed;
                thisPeerData.ownShipData.initialBearing = thisPeerData.otherShipsData.at(thisPeer).legs.at(0).bearing;
            } else {
                thisPeerData.ownShipData.initialSpeed = 0;
                thisPeerData.ownShipData.initialBearing = 0;
            }
            //remove thisPeerData.otherShipsData.at(thisPeer)
            thisPeerData.otherShipsData.erase(thisPeerData.otherShipsData.begin()+thisPeer);

            //Send initial scenario information (reliable packet)
            network.sendString(thisPeerData.serialise(false),true,thisPeer);

            //Store the data for this peer
            peerScenarioData.push_back(thisPeerData);

        } else {
            std::cout << "More Simulateur peers than ships available from scenario." << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    //Start main loop, listening for updates from PCs and sending out scenario update, including time handling
    while(device->run())
    {

        driver->beginScene(true, true, Ui::background);

        // Pause, so we don't flood clients with data
        device->sleep(sleepTime);

        // Find current time acceleration
        accelerator = eventReceiver.getAccelerator();

        //Do time handling here.
        currentTime = std::chrono::system_clock::now();
        std::chrono::duration<float> elapsedTime = currentTime-previousTime;
        previousTime = currentTime;

        float deltaTime = accelerator*(std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime)).count()/1000.0;

        scenarioTime += deltaTime;
        absoluteTime = Utilities::round(scenarioTime) + scenarioOffsetTime;

        std::string timeString = makeTimeString(absoluteTime,scenarioOffsetTime,scenarioTime,accelerator);

        //for each peer
        for(unsigned int thisPeer = 0; thisPeer<numberOfPeers; thisPeer++ ) {

            std::string stringToSend = "BC";

            //0: Time info
            stringToSend.append(timeString);
            stringToSend.append("#");

            //1: Own ship info: Not used
            stringToSend.append("0#");

            //2: Number of other ships: Size of master other ships list -1, as we don't count the one being used as our own ship
            stringToSend.append(Utilities::lexical_cast<std::string>(numberOfOtherShips));
            stringToSend.append(",");
            stringToSend.append("0,0,"); //Number of buoys and MOB, values not used
            stringToSend.append(Utilities::lexical_cast<std::string>(linesData.getNumberOfOtherLines(thisPeer))); // Number of lines (mooring/towing)
            stringToSend.append("#"); //Terminate number info

            //3: Info on each other ship
            //For each Other, terminated with '#' at end of list
            //    PosX,PosZ,Heading,speed (kts),0(SART), 0 (Number of legs, 0 as we don't need leg info in multiplayer)|
            std::string otherShipsString;
            for(unsigned int i = 0; i < (numberOfOtherShips+1); i++) {
                if (i!=thisPeer) {
                    irr::f32 thisOtherShipX = 0;
                    irr::f32 thisOtherShipZ = 0;
                    irr::f32 thisOtherShipSpeed = 0;
                    irr::f32 thisOtherShipBearing = 0;
                    irr::f32 thisOtherShipRateOfTurn = 0;

                    shipPositionData.getShipPosition(i,
                                                     scenarioTime,
                                                     thisOtherShipX,
                                                     thisOtherShipZ,
                                                     thisOtherShipSpeed,
                                                     thisOtherShipBearing,
                                                     thisOtherShipRateOfTurn);

                    otherShipsString.append(Utilities::lexical_cast<std::string>(thisOtherShipX));
                    otherShipsString.append(",");
                    otherShipsString.append(Utilities::lexical_cast<std::string>(thisOtherShipZ));
                    otherShipsString.append(",");
                    otherShipsString.append(Utilities::lexical_cast<std::string>(thisOtherShipBearing));
                    otherShipsString.append(",");
                    otherShipsString.append(Utilities::lexical_cast<std::string>(thisOtherShipSpeed*MPS_TO_KTS));
                    otherShipsString.append(",");

                    // TODO: Send Rate of turn here
                    otherShipsString.append(Utilities::lexical_cast<std::string>(thisOtherShipRateOfTurn));
                    otherShipsString.append(",");

                    otherShipsString.append("0,0,0,0"); //SART enabled, MMSI, number of legs,leg info. TODO: Can we get MMSI
                    otherShipsString.append("|"); //End of other ship record
                }
            }
            //strip trailing '|' if present
            if(otherShipsString.length()>0) {
                otherShipsString = otherShipsString.substr(0,otherShipsString.length()-1);
            }
            stringToSend.append(otherShipsString);
            stringToSend.append("#");

            //Intermediate entries need to be present, but values aren't used
            stringToSend.append("4#5#6#7#8#9#10#");

            //11: Lines information (mooring/towing)
            std::string linesString = linesData.getLineDataString(thisPeer);
            //strip trailing '|' if present
            if(linesString.length()>0) {
                linesString = linesString.substr(0,linesString.length()-1);
            }
            stringToSend.append(linesString);
            stringToSend.append("#");

            //12: 13 basic records in data sent, entry 12 is engine and wheel data for secondary controls, not needed, so send blank entry
            stringToSend.append("12");

            //std::cout << stringToSend << std::endl;

            network.sendString(stringToSend,false,thisPeer);

            //std::cout << "Sending to peer " << thisPeer << " Message:" << stringToSend << std::endl;

            /*
            For multiplayer, only actually uses info from records 0 (time), 2 (Number of entities) & 3 (Other ship info). BC Checks number of entries, so just need dummies
            Format is (with added newlines):
            BC

            (0) timestamp (unix), timestamp of start of first scenario day,
            time since start of first scenario day (float), accelerator#

            (1, own ship data not used in multiplayer, so can leave as 0#)
            Pos x, Pos z, heading, rate of turn, pitch, roll, SOG (knots), COG#

            (2) Number other, number buoys, number MOB (0)#

            (3) For each Other, terminated with '#' at end of list
                PosX,PosZ,Heading,speed (kts),0(SART), 0 (Number of legs, 0 as we don't need leg info in multiplayer)|

            Records 4 to 10 not used (separate with '#')

            (11) Lines information...


            */

            network.listenForMessages();
            std::string receivedMessage = network.getLatestMessage(thisPeer);
            if (receivedMessage.length() > 3 && receivedMessage.substr(0,3) == "MPF") { //Starts with 'MPF' for multiplayer feedback
                receivedMessage = receivedMessage.substr(3,receivedMessage.length()-3); //Strip 'MPF'
                std::vector<std::string> splitMessage = Utilities::split(receivedMessage,'#');
                //Store information
                if (splitMessage.size() == 7) {
                    irr::f32 thisOtherShipX = Utilities::lexical_cast<irr::f32>(splitMessage.at(0));
                    irr::f32 thisOtherShipZ = Utilities::lexical_cast<irr::f32>(splitMessage.at(1));
                    irr::f32 thisOtherShipBearing = Utilities::lexical_cast<irr::f32>(splitMessage.at(2));
                    irr::f32 thisOtherShipRateOfTurn = Utilities::lexical_cast<irr::f32>(splitMessage.at(3)); // deg/s
                    irr::f32 thisOtherShipSpeed = Utilities::lexical_cast<irr::f32>(splitMessage.at(4));
                    irr::f32 thisOtherShipTime = Utilities::lexical_cast<irr::f32>(splitMessage.at(5));
                    shipPositionData.setShipPosition(thisPeer,thisOtherShipTime,thisOtherShipX,thisOtherShipZ,thisOtherShipSpeed,thisOtherShipBearing,thisOtherShipRateOfTurn);

                    // Lines data, from splitMessage.at(6)
                    std::vector<std::string> linesDataString = Utilities::split(splitMessage.at(6),'|');

                    //std::cout << "Message in from " << thisPeer << ": " << splitMessage.at(6) << std::endl;
                    
                    // If necessary, add or delete line
                    linesData.setLineDataSize(thisPeer, linesDataString.size());
                    
                    // Update line information
                    for (int lineID = 0; lineID < linesDataString.size(); lineID++) {
                        std::vector<std::string> thisLineData = Utilities::split(linesDataString.at(lineID),',');
                        // Check number of elements for line data line
                        if (thisLineData.size() == 16) {
                            irr::f32 thisStartX = Utilities::lexical_cast<irr::f32>(thisLineData.at(0));
                            irr::f32 thisStartY = Utilities::lexical_cast<irr::f32>(thisLineData.at(1));
                            irr::f32 thisStartZ = Utilities::lexical_cast<irr::f32>(thisLineData.at(2));
                            irr::f32 thisEndX = Utilities::lexical_cast<irr::f32>(thisLineData.at(3));
                            irr::f32 thisEndY = Utilities::lexical_cast<irr::f32>(thisLineData.at(4));
                            irr::f32 thisEndZ = Utilities::lexical_cast<irr::f32>(thisLineData.at(5));
                            int thisStartType = Utilities::lexical_cast<int>(thisLineData.at(6));
                            int thisEndType = Utilities::lexical_cast<int>(thisLineData.at(7));
                            int thisStartID = Utilities::lexical_cast<int>(thisLineData.at(8));
                            int thisEndID = Utilities::lexical_cast<int>(thisLineData.at(9));
                            irr::f32 thisNominalLength = Utilities::lexical_cast<irr::f32>(thisLineData.at(10));
                            irr::f32 thisBreakingTension = Utilities::lexical_cast<irr::f32>(thisLineData.at(11));
                            irr::f32 thisBreakingStrain = Utilities::lexical_cast<irr::f32>(thisLineData.at(12));
                            irr::f32 thisNominalShipMass = Utilities::lexical_cast<irr::f32>(thisLineData.at(13));
                            int thisKeepSlack = Utilities::lexical_cast<int>(thisLineData.at(14));
                            int thisHeaveIn = Utilities::lexical_cast<int>(thisLineData.at(15));

                            // Modify start and end data to internal IDs (e.g. all ownShip will change to otherShip)
                            if (thisStartType == 1) {
                                // Own ship
                                thisStartType = 2;
                                thisStartID = thisPeer;
                            } else if (thisStartType == 2) {
                                // Other ship. Essential to use else if, to avoid accidentally changing own ship again
                                // Convert from local thisStartID (as used by thisPeer) to the overall list of other ships in the multiplayer scenario
                                if (thisStartID >= thisPeer) {
                                    thisStartID = thisStartID + 1;
                                }
                            }
                            if (thisEndType == 1) {
                                // Own ship
                                thisEndType = 2;
                                thisEndID = thisPeer;
                            } else if (thisEndType == 2) {
                                // Other ship. Essential to use else if, to avoid accidentally changing own ship again
                                // Convert from local thisStartID (as used by thisPeer) to the overall list of other ships in the multiplayer scenario
                                if (thisEndID >= thisPeer) {
                                    thisEndID = thisEndID + 1;
                                }
                            }

                            linesData.setLineData(thisPeer, lineID, 
                                                  thisStartType, thisEndType, thisStartID, thisEndID, 
                                                  thisKeepSlack,
                                                  thisHeaveIn, 
                                                  thisStartX, thisStartY, thisStartZ, 
                                                  thisEndX, thisEndY, thisEndZ, 
                                                  thisNominalLength, 
                                                  thisBreakingTension, 
                                                  thisBreakingStrain, 
                                                  thisNominalShipMass);
                        }
                    }
                    // End lines data
                } // Correct number of entries in MPF
            } // MPF record
        } //End of loop for each peer


        //Update the live view: clock, and each station's ship as last reported.
        std::vector<HubDashboard::Station> stations;
        for(unsigned int i = 0; i<numberOfPeers; i++ ) {
            irr::f32 thisOtherShipX = 0;
            irr::f32 thisOtherShipZ = 0;
            irr::f32 thisOtherShipSpeed = 0;
            irr::f32 thisOtherShipBearing = 0;
            irr::f32 thisOtherShipRateOfTurn = 0;

            shipPositionData.getShipPosition(i,scenarioTime,thisOtherShipX,thisOtherShipZ,thisOtherShipSpeed,thisOtherShipBearing,thisOtherShipRateOfTurn);
            HubDashboard::Station station;
            const std::string host = network.getPeerName(i);
            const std::string ship = i < peerScenarioData.size() ? peerScenarioData[i].ownShipData.ownShipName : std::string();
            station.host = std::wstring(host.begin(), host.end());
            station.ship = std::wstring(ship.begin(), ship.end());
            station.speedKts = thisOtherShipSpeed*MPS_TO_KTS;
            station.heading = thisOtherShipBearing;
            stations.push_back(station);
        }
        const std::string clockText = Utilities::timestampToString(absoluteTime, "%H:%M:%S");
        const std::string dateText = Utilities::timestampToString(absoluteTime, "%d/%m/%Y");
        dashboard->update(std::wstring(clockText.begin(), clockText.end()), std::wstring(dateText.begin(), dateText.end()),
            accelerator, stations, linesData.getNumberOfLines());

        smgr->drawAll();
        device->getGUIEnvironment()->drawAll();
        driver->endScene();

    } //End of main loop

    return(0);

}
