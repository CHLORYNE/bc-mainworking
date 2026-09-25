//Common launcher program
//This just launches Bridge Command or
//Map Controller executable depending
//on which button the user presses

/*
* Icons from :
* https://github.com/dubdubdubco/iconicicons.git
* CC0 Public domain
*/

#ifdef _MSC_VER
#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#include "irrlicht.h"
#include <iostream>
#include <thread>
#include "../IniFile.hpp"
#include "../Lang.hpp"
#include "../Utilities.hpp"
#include "../Constants.hpp"

//headers for execl
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

//Mac OS:
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

// Irrlicht Namespaces
//using namespace irr;

const int FONT_SIZE_DEFAULT = 12;

//Global definition for ini logger
namespace IniFile {
    irr::ILogger* irrlichtLogger = 0;
}

const irr::s32 BC_BUTTON = 1;
const irr::s32 MC_BUTTON = 2;
const irr::s32 RP_BUTTON = 3;
const irr::s32 ED_BUTTON = 4;
const irr::s32 MH_BUTTON = 5;
const irr::s32 INI_BC_BUTTON = 6;
const irr::s32 INI_MC_BUTTON = 7;
//const irr::s32 INI_RP_BUTTON = 8;
const irr::s32 INI_MH_BUTTON = 9;
const irr::s32 DOC_BUTTON = 10;
const irr::s32 USER_BUTTON = 11;
const irr::s32 EXIT_BUTTON = 12;

std::string userFolder;

//Event receiver: This does the actual launching
class Receiver : public irr::IEventReceiver
{
public:
    Receiver() {}

    virtual bool OnEvent(const irr::SEvent& event)
    {
        if (event.EventType == irr::EET_GUI_EVENT) {
            if (event.GUIEvent.EventType == irr::gui::EGET_BUTTON_CLICKED) {
                irr::s32 id = event.GUIEvent.Caller->getID();

                if (id == EXIT_BUTTON) {
                    exit(EXIT_SUCCESS);
                }

#ifndef _WIN32
                int pid = fork();  // posix only (GNU/Linux, MacOS)
                if (pid > 0) return false;
#endif

                if (id == BC_BUTTON) {
#ifdef _WIN32
                    ShellExecute(NULL, NULL, "Simulator-nav.exe", NULL, NULL, SW_SHOW);
                    //_execl("./bridgecommand-bc.exe", "bridgecommand-bc.exe", NULL);
#else
#ifdef __APPLE__
                    //APPLE
                    execl("../MacOS/bc.app/Contents/MacOS/bc", "bc", NULL);
#else
                    //Other (assumed posix)
                    execl("./Simulator-bc", "Simulator-bc", NULL);
#endif
#endif
                }
                if (id == MC_BUTTON) {
#ifdef _WIN32
                    ShellExecute(NULL, NULL, "Simulator-mc.exe", NULL, NULL, SW_SHOW);
                    //_execl("./bridgecommand-mc.exe", "bridgecommand-mc.exe", NULL);
#else
#ifdef __APPLE__
                    //APPLE
                    execl("../MacOS/mc.app/Contents/MacOS/mc", "mc", NULL);
#else
                    //Other (assumed posix)
                    execl("./Simulator-mc", "Simulator-mc", NULL);
#endif
#endif
                }
                if (id == RP_BUTTON) {
#ifdef _WIN32
                    ShellExecute(NULL, NULL, "Simulator-rp.exe", NULL, NULL, SW_SHOW);
                    //_execl("./bridgecommand-rp.exe", "bridgecommand-rp.exe", NULL);
#else
#ifdef __APPLE__
                    //APPLE
                    execl("../MacOS/rp.app/Contents/MacOS/rp", "rp", NULL);
#else
                    //Other (assumed posix)
                    execl("./Simulator-rp", "Simulator-rp", NULL);
#endif
#endif
                }
                if (id == ED_BUTTON) {
#ifdef _WIN32
                    ShellExecute(NULL, NULL, "Simulator-ed.exe", NULL, NULL, SW_SHOW);
                    //_execl("./bridgecommand-ed.exe", "bridgecommand-ed.exe", NULL);
#else
#ifdef __APPLE__
                    //APPLE
                    execl("../MacOS/ed.app/Contents/MacOS/ed", "ed", NULL);
#else
                    //Other (assumed posix)
                    execl("./Simulator-ed", "Simulator-ed", NULL);
#endif
#endif
                }
                if (id == MH_BUTTON) {
#ifdef _WIN32
                    ShellExecute(NULL, NULL, "Simulator-mh.exe", NULL, NULL, SW_SHOW);
                    //_execl("./bridgecommand-mh.exe", "bridgecommand-mh.exe", NULL);
#else
#ifdef __APPLE__
                    //APPLE
                    execl("../MacOS/mh.app/Contents/MacOS/mh", "mh", NULL);
#else
                    //Other (assumed posix)
                    execl("./Simulator-mh", "Simulator-mh", NULL);
#endif
#endif
                }
                if (id == INI_BC_BUTTON) {
#ifdef _WIN32
                    ShellExecute(NULL, NULL, "Simulator-ini.exe", NULL, NULL, SW_SHOW);
                    //_execl("./bridgecommand-ini.exe", "bridgecommand-ini.exe", NULL);
#else
#ifdef __APPLE__
                    //APPLE
                    execl("../MacOS/ini.app/Contents/MacOS/ini", "ini", NULL);
#else
                    //Other (assumed posix)
                    execl("./Simulator-ini", "Simulator-ini", NULL);
#endif
#endif
                }
                if (id == INI_MC_BUTTON) {
#ifdef _WIN32
                    ShellExecute(NULL, NULL, "Simulator-ini.exe", "-M", NULL, SW_SHOW);
                    //_execl("./bridgecommand-ini.exe", "bridgecommand-ini.exe", "-M", NULL);
#else
#ifdef __APPLE__
                    //APPLE
                    execl("../MacOS/ini.app/Contents/MacOS/ini", "ini", "-M", NULL);
#else
                    //Other (assumed posix)
                    execl("./Simulator-ini", "Simulator-ini", "-M", NULL);
#endif
#endif
                }

                if (id == INI_MH_BUTTON) {
#ifdef _WIN32
                    ShellExecute(NULL, NULL, "Simulator-ini.exe", "-H", NULL, SW_SHOW);
                    //_execl("./bridgecommand-ini.exe", "bridgecommand-ini.exe", "-H", NULL);
#else
#ifdef __APPLE__
                    //APPLE
                    execl("../MacOS/ini.app/Contents/MacOS/ini", "ini", "-H", NULL);
#else
                    //Other (assumed posix)
                    execl("./Simulator-ini", "Simulator-ini", "-H", NULL);
#endif
#endif
                }
                if (id == DOC_BUTTON) {
#ifdef _WIN32
                    //CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
                    ShellExecute(NULL, TEXT("open"), TEXT("doc\\index.html"), NULL, NULL, SW_SHOWNORMAL);
                    //Sleep(5000);
                    //exit(EXIT_SUCCESS);
#else
#ifdef __APPLE__
                    //APPLE
                    execl("/usr/bin/open", "open", "../Resources/doc/index.html", NULL);
#else
                    //Other (assumed posix)
#ifdef FOR_DEB
                    execl("/usr/bin/xdg-open", "xdg-open", "/usr/share/doc/Simulator/index.html", NULL);
                    //If execuation gets to this point, it has failed to launch help. Try to fall back to online documentation
                    chdir("/usr/bin"); // If firefox is running in a snap or similar, launching can fail if it can't access the current dir
                    execl("/usr/bin/xdg-open", "xdg-open", "https://www.bridgecommand.co.uk/Documentation", NULL);
#else
                    execl("/usr/bin/xdg-open", "xdg-open", "doc/index.html", NULL);
                    //If execuation gets to this point, it has failed to launch help. Try to fall back to online documentation
                    chdir("/usr/bin"); // If firefox is running in a snap or similar, launching can fail if it can't access the current dir
                    execl("/usr/bin/xdg-open", "xdg-open", "https://www.bridgecommand.co.uk/Documentation", NULL);
#endif // FOR_DEB
#endif
#endif
                }

            }
        }
        if (event.EventType == irr::EET_KEY_INPUT_EVENT) {
            if (event.KeyInput.Key == irr::KEY_ESCAPE) {
                exit(EXIT_SUCCESS);
            }
        }
        return false;
    }
};

int main(int argc, char** argv)
{

    if ((argc > 1) && (strcmp(argv[1], "--version") == 0)) {
        std::cout << LONGVERSION << std::endl;
        exit(EXIT_SUCCESS);
    }

#ifdef FOR_DEB
    chdir("/usr/share/Simulator");
#endif // FOR_DEB

    //Mac OS:
    //Find starting folder
#ifdef __APPLE__
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
    //change up from BridgeCommand.app/Contents/MacOS to ../Resources
    exeFolderPath.append("/../Resources");
    //change to this path now
    chdir(exeFolderPath.c_str());
    //Note, we use this again after the createDevice call
#endif

//User read/write location - look in here first and the exe folder second for files
    userFolder = Utilities::getUserDir();

    //Read basic ini settings
    std::string iniFilename = "bc5.ini";
    //Use local ini file if it exists
    if (Utilities::pathExists(userFolder + iniFilename)) {
        iniFilename = userFolder + iniFilename;
    }

    std::string modifier = IniFile::iniFileToString(iniFilename, "lang");
    if (modifier.length() == 0) {
        modifier = "en"; //Default
    }
    std::string languageFile = "languageLauncher-";
    languageFile.append(modifier);
    languageFile.append(".txt");
    if (Utilities::pathExists(userFolder + languageFile)) {
        languageFile = userFolder + languageFile;
    }

    Lang language(languageFile);

    int fontSize = FONT_SIZE_DEFAULT;
    float fontScale = IniFile::iniFileTof32(iniFilename, "font_scale");
    if (fontScale > 1) {
        fontSize = (int)(fontSize * fontScale + 0.5);
    }
    else {
        fontScale = 1.0;
    }

    irr::u32 graphicsWidth = 1000;
    irr::u32 graphicsHeight = 600;
    irr::u32 graphicsDepth = 32;
    bool fullScreen = false;

    irr::IrrlichtDevice* device = irr::createDevice(irr::video::EDT_OPENGL, irr::core::dimension2d<irr::u32>(graphicsWidth, graphicsHeight), graphicsDepth, fullScreen, false, false, 0);
    irr::video::IVideoDriver* driver = device->getVideoDriver();


#ifdef __APPLE__
    //Mac OS - cd back to original dir - seems to be changed during createDevice
    irr::io::IFileSystem* fileSystem = device->getFileSystem();
    if (fileSystem == 0) {
        exit(EXIT_FAILURE); //Could not get file system TODO: Message for user
        std::cout << "Could not get filesystem" << std::endl;
    }
    fileSystem->changeWorkingDirectoryTo(exeFolderPath.c_str());
#endif

    
    //icon - kyara 
    device->setWindowCaption(L"Simulateur de Navigation Maritime");

    // --- ADD THIS BLOCK TO LOAD YOUR CUSTOM WINDOW ICON ---
#ifdef _WIN32
    // Load your custom .ico file from the media folder
    HICON hIcon = (HICON)LoadImageA(NULL, "media/myIcon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    if (hIcon) {
        // Extract the native Windows window handle (HWND) from the Irrlicht engine
        irr::video::SExposedVideoData videoData = driver->getExposedVideoData();
        HWND hwnd = reinterpret_cast<HWND>(videoData.OpenGLWin32.HWnd);

        // Attach the icon to the window's title bar and taskbar
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }
#endif
    // ------------------------------------------------------

    irr::gui::IGUISkin* newskin = device->getGUIEnvironment()->createSkin(irr::gui::EGST_WINDOWS_CLASSIC);
    device->getGUIEnvironment()->setSkin(newskin);
    //BUTTONS MODIFICATION 
   
    irr::gui::IGUISkin* skin = device->getGUIEnvironment()->getSkin();

   
// Custom color palette from kyara
    irr::video::SColor deepestBlue(255, 165, 224, 255);        // Light blue
    irr::video::SColor mediumBlueGray(255, 142, 210, 225); // #427AA1  
    irr::video::SColor lightBlueWhite(255, 235, 242, 250); // #EBF2FA
    irr::video::SColor blackText(255, 0, 0, 0);      // Black nigger

    // 1. Set Button Background to Deep Navy Blue
    skin->setColor(irr::gui::EGDC_3D_FACE, deepestBlue);

    // 2. Set Button Text to black
    skin->setColor(irr::gui::EGDC_BUTTON_TEXT, blackText);

    // 3. Highlight and Shadow colors to match the Navy buttons
    skin->setColor(irr::gui::EGDC_3D_HIGH_LIGHT, mediumBlueGray);
    skin->setColor(irr::gui::EGDC_3D_SHADOW, irr::video::SColor(255, 2, 15, 30)); // Very dark blue for 3D shadow effect
    skin->setColor(irr::gui::EGDC_3D_DARK_SHADOW, irr::video::SColor(255, 0, 0, 0));
    skin->setColor(irr::gui::EGDC_3D_LIGHT, deepestBlue);

    // Additional GUI element colors for complete customization
    skin->setColor(irr::gui::EGDC_3D_DARK_SHADOW, deepestBlue);
    skin->setColor(irr::gui::EGDC_3D_LIGHT, mediumBlueGray);

    // BUTTONS MODIFICATION 
    // Set the background color to match your classic Nautitech Deep Blue
    irr::video::SColor colBg(255, 0, 70, 130);

    // --- NEW FULLSCREEN BACKGROUND ---
    irr::video::ITexture* bgTex = driver->getTexture("media/bg_main.png");

    if (bgTex) {
        irr::gui::IGUIImage* bgImg = device->getGUIEnvironment()->addImage(irr::core::rect<irr::s32>(0, 0, graphicsWidth, graphicsHeight));
        bgImg->setImage(bgTex);
        bgImg->setScaleImage(true); // Stretches to fit the window perfectly
        bgImg->setEnabled(false);   
    }



    // Set standard button text to black for classic readability
    skin->setColor(irr::gui::EGDC_BUTTON_TEXT, irr::video::SColor(255, 0, 0, 0));



    // --- ADD THIS FONT LOADING BLOCK ---
    irr::gui::IGUIFont* customFont = device->getGUIEnvironment()->getFont("media/lucida.xml");
    if (customFont) {
        skin->setFont(customFont);
    }
    // -----------------------------------







    // 3. Horizontal Grid Layout for Classic Buttons
    int startX = 50;
    int startY = 310;
    int btnW = 240;
    int btnH = 35;
    int gapX = 30;

    // Trim trailing/leading whitespace from language strings so button text centers properly
    auto trimLabel = [](std::wstring s) -> std::wstring {
        size_t start = s.find_first_not_of(L" \t\r\n");
        size_t end   = s.find_last_not_of(L" \t\r\n");
        return (start == std::wstring::npos) ? L"" : s.substr(start, end - start + 1);
    };
    auto T = [&](const std::string& key) -> irr::core::stringw {
        std::wstring ws = language.translate(key.c_str()).c_str();
        return irr::core::stringw(trimLabel(ws).c_str());
    };

    // Left group: c1 at x=50
    int c1 = startX;
    // Right group: mirrored, flush against right edge (margin=50)
    int c2r = (int)graphicsWidth - startX - btnW; // = 710

    int c1Y  = startY;
    int c2rY = startY;

    // Left Column — 3 launcher buttons
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c1, c1Y, c1 + btnW, c1Y + btnH), 0, BC_BUTTON,    T("startBC").c_str()); c1Y += btnH;
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c1, c1Y, c1 + btnW, c1Y + btnH), 0, ED_BUTTON,    T("startED").c_str()); c1Y += btnH;
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c1, c1Y, c1 + btnW, c1Y + btnH), 0, MH_BUTTON,    T("startMH").c_str());

    // Right Column — 3 settings buttons
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c2r, c2rY, c2r + btnW, c2rY + btnH), 0, INI_BC_BUTTON, T("startINIBC").c_str()); c2rY += btnH;
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c2r, c2rY, c2r + btnW, c2rY + btnH), 0, INI_MC_BUTTON, T("startINIMC").c_str()); c2rY += btnH;
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c2r, c2rY, c2r + btnW, c2rY + btnH), 0, INI_MH_BUTTON, T("startINIMH").c_str());

    // Exit button — centered at the bottom
    int exitW = 200;
    int exitH = 35;
    int exitX = (int)(graphicsWidth - exitW) / 2;
    int exitY = (int)graphicsHeight - exitH - 40;
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(exitX, exitY, exitX + exitW, exitY + exitH), 0, EXIT_BUTTON, T("leave").c_str());

    // 4. Version Info
    std::string version = "v" + LONGVERSION;
    irr::core::stringw wVer(version.c_str());
    device->getGUIEnvironment()->addStaticText(wVer.c_str(), irr::core::rect<irr::s32>(20, graphicsHeight - 30, 200, graphicsHeight), false);

    device->getGUIEnvironment()->setFocus(device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(BC_BUTTON));

    Receiver receiver;
    device->setEventReceiver(&receiver);

#ifdef FOR_DEB
    chdir("/usr/bin");
#endif // FOR_DEB

    // Render loop
    while (device->run()) {
        driver->beginScene(irr::video::ECBF_COLOR | irr::video::ECBF_DEPTH, colBg);
        device->getGUIEnvironment()->drawAll();
        driver->endScene();
        device->sleep(100);
    }

    return EXIT_SUCCESS;
}