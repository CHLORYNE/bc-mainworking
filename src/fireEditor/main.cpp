/* SCENARIO INCENDIE - fire scenario editor (Simulator-fe).
   A separate editor for the fire / abandon-ship / SAR exercises: pick a chart for a new exercise,
   or open an existing one, then lay it out on the map. Writes a normal scenario folder plus
   incident.ini, which the simulator reads.
   Source kept ASCII: accented text is written with \u escapes. */
#include "irrlicht.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "../IniFile.hpp"
#include "../Utilities.hpp"
#include "FireEditor.hpp"
#include "FireMenu.hpp"
#include "../chartView/ChartView.hpp"
#include "FireScenario.hpp"

#ifdef _MSC_VER
#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX   // keep std::min / std::max usable
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <unistd.h>
#endif

#ifdef __linux__
#include <unistd.h>
#endif

// Global for the ini reader to reach the irrlicht logger, as in the other tools.
namespace IniFile {
    irr::ILogger* irrlichtLogger = 0;
}

namespace {

std::wstring wide(const std::string& s)
{
    std::wstring w;
    for (size_t i = 0; i < s.size(); i++) { w += (wchar_t)(unsigned char)s[i]; }
    return w;
}

void applySkin(irr::gui::IGUIEnvironment* env)
{
    // Same palette as the standard scenario editor.
    irr::gui::IGUISkin* skin = env->getSkin();
    irr::video::SColor bgDark(240, 30, 34, 43);
    irr::video::SColor panelColor(255, 45, 52, 60);
    irr::video::SColor borderDark(255, 20, 24, 30);
    irr::video::SColor borderLight(255, 65, 75, 85);
    irr::video::SColor textMain(255, 240, 245, 250);
    irr::video::SColor highlightBlue(255, 52, 152, 219);
    irr::video::SColor editBg(255, 20, 24, 30);
    skin->setColor(irr::gui::EGDC_WINDOW, bgDark);
    skin->setColor(irr::gui::EGDC_3D_FACE, panelColor);
    skin->setColor(irr::gui::EGDC_3D_SHADOW, borderDark);
    skin->setColor(irr::gui::EGDC_3D_DARK_SHADOW, borderDark);
    skin->setColor(irr::gui::EGDC_3D_HIGH_LIGHT, borderLight);
    skin->setColor(irr::gui::EGDC_3D_LIGHT, borderLight);
    skin->setColor(irr::gui::EGDC_BUTTON_TEXT, textMain);
    skin->setColor(irr::gui::EGDC_GRAY_TEXT, borderLight);
    skin->setColor(irr::gui::EGDC_TOOLTIP, textMain);
    skin->setColor(irr::gui::EGDC_HIGH_LIGHT, highlightBlue);
    skin->setColor(irr::gui::EGDC_HIGH_LIGHT_TEXT, irr::video::SColor(255, 0, 0, 0));
    skin->setColor(irr::gui::EGDC_WINDOW_SYMBOL, highlightBlue);
    skin->setColor(irr::gui::EGDC_EDITABLE, editBg);
    skin->setColor(irr::gui::EGDC_FOCUSED_EDITABLE, borderLight);
    skin->setColor(irr::gui::EGDC_GRAY_EDITABLE, bgDark);
    skin->setColor(irr::gui::EGDC_ACTIVE_BORDER, highlightBlue);
    skin->setDefaultText(irr::gui::EGDT_MSG_BOX_YES, L"Oui");
    skin->setDefaultText(irr::gui::EGDT_MSG_BOX_NO, L"Non");
    skin->setDefaultText(irr::gui::EGDT_MSG_BOX_CANCEL, L"Annuler");
}

void showMessage(irr::IrrlichtDevice* device, const wchar_t* text)
{
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::gui::IGUIEnvironment* env = device->getGUIEnvironment();
    irr::core::dimension2du s = driver->getScreenSize();
    irr::gui::IGUIStaticText* t = env->addStaticText(text, irr::core::recti(0, 0, (irr::s32)s.Width, (irr::s32)s.Height), false, true);
    t->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
    if (device->run()) {
        driver->beginScene(true, true, irr::video::SColor(255, 30, 34, 43));
        env->drawAll();
        driver->endScene();
    }
    t->remove();
}

} // namespace

int main(int argc, char** argv)
{
    (void)argc; (void)argv;
#ifdef FOR_DEB
    chdir("/usr/share/bridgecommand");
#endif
#ifdef __APPLE__
    // Run from BridgeCommand.app/Contents/Resources, as the other tools do.
    char exePath[1024];
    uint32_t pathSize = sizeof(exePath);
    std::string exeFolderPath = "";
    if (_NSGetExecutablePath(exePath, &pathSize) == 0) {
        std::string p(exePath);
        size_t pos = p.find_last_of("\\/");
        if (pos != std::string::npos) { exeFolderPath = p.substr(0, pos); }
    }
    exeFolderPath.append("/../../../../Resources");
    chdir(exeFolderPath.c_str());
#endif

    std::string userFolder = Utilities::getUserDir();
    std::string iniFilename = "map.ini";
    if (Utilities::pathExists(userFolder + iniFilename)) { iniFilename = userFolder + iniFilename; }

    int fontSize = 12;
    float fontScale = IniFile::iniFileTof32(iniFilename, "font_scale");
    if (fontScale > 1) { fontSize = (int)(fontSize * fontScale + 0.5); }
    else { fontScale = 1.0; }

    irr::u32 graphicsWidth = IniFile::iniFileTou32(iniFilename, "graphics_width");
    irr::u32 graphicsHeight = IniFile::iniFileTou32(iniFilename, "graphics_height");
    irr::u32 graphicsDepth = IniFile::iniFileTou32(iniFilename, "graphics_depth");
    bool fullScreen = (IniFile::iniFileTou32(iniFilename, "graphics_mode") == 1);

    irr::core::dimension2d<irr::u32> deskres;
#ifdef _WIN32
    deskres.Width = GetSystemMetrics(SM_CXSCREEN);
    deskres.Height = GetSystemMetrics(SM_CYSCREEN);
#else
    irr::IrrlichtDevice* nulldevice = irr::createDevice(irr::video::EDT_NULL);
    deskres = nulldevice->getVideoModeList()->getDesktopResolution();
    nulldevice->drop();
#endif
    // The map wants room: default to most of the screen.
    if (graphicsWidth == 0) { graphicsWidth = (irr::u32)(deskres.Width * 0.92); }
    if (graphicsHeight == 0) { graphicsHeight = (irr::u32)(deskres.Height * 0.88); }
    if (graphicsWidth < 1000) { graphicsWidth = 1000; }
    if (graphicsHeight < 700) { graphicsHeight = 700; }

    irr::IrrlichtDevice* device = irr::createDevice(irr::video::EDT_OPENGL,
        irr::core::dimension2d<irr::u32>(graphicsWidth, graphicsHeight), graphicsDepth, fullScreen, false, false, 0);
    if (!device) {
        std::cerr << "Could not create the graphics device" << std::endl;
        return 1;
    }
    IniFile::irrlichtLogger = device->getLogger();
    irr::gui::IGUIEnvironment* env = device->getGUIEnvironment();

#ifdef __APPLE__
    device->getFileSystem()->changeWorkingDirectoryTo(exeFolderPath.c_str());
#endif

    std::string fontName = IniFile::iniFileToString(iniFilename, "font");
    std::string fontPath = "media/fonts/" + fontName + "/" + fontName + "-" + std::to_string(fontSize) + ".xml";
    irr::gui::IGUIFont* font = env->getFont(fontPath.c_str());
    if (font) { env->getSkin()->setFont(font); }
    else { std::cout << "Could not load font, using fallback" << std::endl; }
    applySkin(env);

#ifdef _WIN32
    //As the scenario editor does on its first run: copy the scenarios into the user folder. Without it
    //a new fire scenario was saved next to the program (refused, or redirected, under Program Files),
    //and once the user folder existed the simulator, which then lists only that one, lost it.
    if (!Utilities::pathExists(userFolder + "Scenarios")) {
        std::string from = "Scenarios";
        from.append(1, '\0');                       //the list ends with a double 0
        std::string to = userFolder + "Scenarios";
        std::replace(to.begin(), to.end(), '/', '\\');
        to.append(1, '\0');
        SHFILEOPSTRUCTA op = {};
        op.wFunc = FO_COPY;
        op.pFrom = from.c_str();
        op.pTo = to.c_str();
        op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR | FOF_SILENT;
        SHFileOperationA(&op);
    }
#endif
    std::string scenariosPath = "Scenarios/";   // same lookup as the simulator
    if (Utilities::pathExists(userFolder + scenariosPath)) { scenariosPath = userFolder + scenariosPath; }

    std::vector<std::string> ownTypes, otherTypes;
    listSubdirectories(device, ownTypes, "Models/Ownship/");
    listSubdirectories(device, ownTypes, userFolder + "Models/Ownship/");
    listSubdirectories(device, otherTypes, "Models/Othership/");
    listSubdirectories(device, otherTypes, userFolder + "Models/Othership/");
    std::sort(ownTypes.begin(), ownTypes.end());
    std::sort(otherTypes.begin(), otherTypes.end());

    // SAR boats: other-ship models certified FireFighting=1 only. The burning ship defaults to
    // the first model that is not one of those.
    std::vector<std::string> rescueTypes;
    std::string defaultCasualtyType = otherTypes.empty() ? "" : otherTypes[0];
    bool casualtyTypeFound = false;
    for (size_t i = 0; i < otherTypes.size(); i++) {
        if (FireScenario::isRescueModel(otherTypes[i])) { rescueTypes.push_back(otherTypes[i]); }
        else if (!casualtyTypeFound) { defaultCasualtyType = otherTypes[i]; casualtyTypeFound = true; }
    }

    std::wstring message =
        L"Cet \u00E9diteur pr\u00E9pare les exercices d'incendie : navire en feu, chronologie (abandon, naufrage), naufrag\u00E9s, "
        L"vedettes SAR avec leurs routes et h\u00E9licopt\u00E8res. Les exercices marqu\u00E9s [incendie] ont d\u00E9j\u00E0 leurs r\u00E9glages.";

    while (device->run()) {
        FireScenario scenario;
        FireMenu::Choice choice;
        std::string chosenWorld, chosenScenario;
        {
            FireMenu menu(device, scenariosPath, message);
            choice = menu.run();
            chosenWorld = menu.chosenWorld();
            chosenScenario = menu.chosenScenario();
            if (choice == FireMenu::Choice_Import) { scenario = menu.imported; }
        }
        if (choice == FireMenu::Choice_Quit || !device->run()) { break; }

        showMessage(device, L"Chargement de la carte...");
        ChartView map;
        std::string error;
        bool isNew = (choice != FireMenu::Choice_Open);
        if (choice == FireMenu::Choice_New) {
            if (!map.load(device, chosenWorld, error)) {
                message = L"Impossible d'ouvrir la carte : " + wide(error);
                continue;
            }
            map.setViewport(irr::core::recti(0, 0, (irr::s32)graphicsWidth, (irr::s32)graphicsHeight));
            scenario.makeNew(chosenWorld, map.centre(),
                ownTypes.empty() ? "" : ownTypes[0], defaultCasualtyType, rescueTypes.empty() ? "" : rescueTypes[0], "");
        }
        else {
            if (choice == FireMenu::Choice_Open && !scenario.load(scenariosPath + chosenScenario, chosenScenario, error)) {
                message = L"Impossible d'ouvrir l'exercice : " + wide(error);
                continue;
            }
            if (!map.load(device, scenario.worldName, error)) {
                message = L"Impossible d'ouvrir la carte de l'exercice : " + wide(error);
                continue;
            }
            scenario.applyBuiltInPresetIfInside(map.south, map.west, map.latExtent, map.longExtent);
        }

        bool backToMenu;
        {
            FireEditor editor(device, &scenario, &map, scenariosPath, ownTypes, otherTypes, rescueTypes, isNew);
            device->setEventReceiver(&editor);
            backToMenu = editor.run();
            device->setEventReceiver(0);
        }
        env->clear();
        if (!backToMenu) { break; }
        message = L"Choisissez une carte ou un exercice.";
    }

    device->drop();
    return 0;
}
