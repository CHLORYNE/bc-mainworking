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
//KYARA TOUCHES: keyboard-shortcut sheet, in French or English
const irr::s32 KEYS_BUTTON = 13;
const irr::s32 KEYS_CLOSE_BUTTON = 14;
const irr::s32 KEYS_LANG_BUTTON = 15;
const irr::s32 FE_BUTTON = 16;   // SCENARIO INCENDIE: fire / SAR scenario editor

std::string userFolder;

//=================================================================================================
//KYARA TOUCHES: the keyboard shortcuts of the simulator, shown from the launcher so an instructor
//can read them before starting a session, or show them to the trainees.
//Both languages are held here rather than in languageLauncher-xx.txt on purpose: the sheet must
//read correctly whatever language file is installed, and the two versions have to stay side by
//side so that neither is forgotten when a shortcut changes.
//=================================================================================================
irr::IrrlichtDevice* g_device = 0;
irr::gui::IGUIWindow* g_keysWindow = 0;
bool g_keysFrench = true;

struct KeyRow { const wchar_t* keys; const wchar_t* fr; const wchar_t* en; };

//keys == 0 marks a section heading; its French and English titles follow in the same two fields.
static const KeyRow KEY_ROWS[] = {
    { 0, L"MACHINES (navire classique)", L"ENGINES (conventional ship)" },
    { L"A / Z",            L"Machine b\u00E2bord : plus / moins",            L"Port engine: increase / decrease" },
    { L"S / X",            L"Machine tribord : plus / moins",                L"Starboard engine: increase / decrease" },
    { L"D / C",            L"Les deux machines : plus / moins",              L"Both engines: increase / decrease" },
    { L"V / B",            L"Barre \u00E0 b\u00E2bord / \u00E0 tribord",     L"Wheel to port / to starboard" },

    { 0, L"PROPULSION AZIMUTALE (azipods)", L"AZIMUTH DRIVE (azipods)" },
    { L"A / D",            L"Schottel b\u00E2bord : anti-horaire / horaire", L"Port schottel: anticlockwise / clockwise" },
    { L"W / S",            L"Manette de pouss\u00E9e b\u00E2bord : avant / arri\u00E8re", L"Port thrust lever: ahead / astern" },
    { L"J / L",            L"Schottel tribord : anti-horaire / horaire",     L"Starboard schottel: anticlockwise / clockwise" },
    { L"I / K",            L"Manette de pouss\u00E9e tribord : avant / arri\u00E8re", L"Starboard thrust lever: ahead / astern" },

    { 0, L"VUE ET CAM\u00C9RA", L"VIEW AND CAMERA" },
    { L"Fl\u00E8ches",     L"Regarder en haut / en bas / \u00E0 gauche / \u00E0 droite", L"Look up / down / left / right" },
    { L"Espace",           L"Changer de poste de vue",                      L"Change view position" },
    { L"Maj + Espace",     L"Changer de vue (la vue ne suit plus la barre)", L"Change view (view no longer follows the helm)" },
    { L"Maj + Gauche / Droite", L"Pas de vue \u00E0 gauche / \u00E0 droite", L"Step the view left / right" },
    { L"Ctrl + Haut / Bas",     L"Regarder devant / sur l'arri\u00E8re",     L"Look ahead / astern" },
    { L"Ctrl + Gauche / Droite",L"Regarder sur b\u00E2bord / sur tribord",   L"Look to port / to starboard" },
    { L"Ctrl + Maj + Haut / Bas", L"Avancer / reculer la cam\u00E9ra",       L"Move the camera forwards / backwards" },
    { L"Ctrl + Maj + Espace",  L"Figer / lib\u00E9rer la cam\u00E9ra",       L"Freeze / release the camera" },
    { L"F",                L"Afficher ou masquer l'interface 2D",           L"Show or hide the 2D interface" },

    { 0, L"TEMPS", L"TIME" },
    { L"0",                L"Pause (acc\u00E9l\u00E9ration nulle)",          L"Pause (zero acceleration)" },
    { L"Entr\u00E9e ou 1", L"Temps r\u00E9el (x1)",                          L"Real time (x1)" },
    { L"2 / 3 / 4",        L"x2 / x5 / x15",                                L"x2 / x5 / x15" },
    { L"5 / 6 / 7",        L"x30 / x60 / x3600",                            L"x30 / x60 / x3600" },

    { 0, L"\u00C9CLAIRAGE", L"LIGHTING" },
    { L"Ctrl + Maj + J",   L"\u00C9crans et cadrans : \u00E9teints / tamis\u00E9s / pleins feux", L"Screens and gauges: off / dimmed / full" },
    { L"Ctrl + Maj + K",   L"Feux de pont et de travail : allum\u00E9s / \u00E9teints", L"Deck and working lights: on / off" },

    { 0, L"MAN\u0152UVRES ET EXERCICES", L"MANOEUVRES AND EXERCISES" },
    { L"H",                L"Corne de brume (maintenir la touche)",         L"Horn (hold the key down)" },
    { L"R",                L"Anneaux de port\u00E9e radar : clair / faible / \u00E9teints", L"Radar range rings: bright / dim / off" },
    { L"P",                L"Couper ou r\u00E9tablir l'alarme de proximit\u00E9", L"Mute or restore the proximity alarm" },
    { L"M",                L"Homme \u00E0 la mer",                          L"Man overboard" },
    { L"Ctrl + M",         L"R\u00E9cup\u00E9rer l'homme \u00E0 la mer",     L"Retrieve the man overboard" },
    { L"Ctrl + F",         L"Incendie sur le navire le plus proche",        L"Set fire to the nearest vessel" },
    { L"Ctrl + E",         L"Lance \u00E0 incendie : en action / arr\u00EAt", L"Water monitor: firing / stopped" },
    { L"Ctrl + A",         L"Action suivante des communications de d\u00E9tresse", L"Next distress-communications action" },
    { L"G",                L"Cri de mouette (ambiance)",                    L"Seagull call (ambience)" },

    { 0, L"SORTIE", L"QUITTING" },
    { L"\u00C9chap ou F4", L"Quitter le simulateur",                        L"Quit the simulator" }
};
static const int KEY_ROW_COUNT = sizeof(KEY_ROWS) / sizeof(KEY_ROWS[0]);

void showKeyHelp()
{
    if (!g_device) { return; }
    irr::gui::IGUIEnvironment* env = g_device->getGUIEnvironment();

    //Rebuilt from scratch each time, so switching language is one code path and not two.
    if (g_keysWindow) { g_keysWindow->remove(); g_keysWindow = 0; }

    const irr::s32 sw = (irr::s32)g_device->getVideoDriver()->getScreenSize().Width;
    const irr::s32 sh = (irr::s32)g_device->getVideoDriver()->getScreenSize().Height;
    const irr::s32 w = 880, h = 540;
    const irr::s32 x = (sw - w) / 2, y = (sh - h) / 2;

    g_keysWindow = env->addWindow(irr::core::rect<irr::s32>(x, y, x + w, y + h), true, //modal
        g_keysFrench ? L"Raccourcis clavier du simulateur" : L"Simulator keyboard shortcuts");
    if (g_keysWindow->getCloseButton()) {
        g_keysWindow->getCloseButton()->setVisible(false); //one explicit close button is clearer
    }

    //The list scrolls by itself, so the sheet can grow later without the window changing.
    irr::gui::IGUIListBox* list = env->addListBox(
        irr::core::rect<irr::s32>(10, 34, w - 10, h - 52), g_keysWindow, -1, true);

    for (int i = 0; i < KEY_ROW_COUNT; i++) {
        irr::core::stringw line;
        if (KEY_ROWS[i].keys == 0) {
            //Section heading: a blank line above it separates the blocks with no styling needed.
            if (i > 0) { list->addItem(L""); }
            line = L"== ";
            line += g_keysFrench ? KEY_ROWS[i].fr : KEY_ROWS[i].en;
            line += L" ==";
        }
        else {
            //Pad the key column so the descriptions line up down the list.
            irr::core::stringw keys(KEY_ROWS[i].keys);
            while (keys.size() < 26) { keys += L" "; }
            line = L"  ";
            line += keys;
            line += g_keysFrench ? KEY_ROWS[i].fr : KEY_ROWS[i].en;
        }
        list->addItem(line.c_str());
    }

    //The toggle always names the language it would switch TO.
    env->addButton(irr::core::rect<irr::s32>(10, h - 44, 210, h - 12), g_keysWindow,
        KEYS_LANG_BUTTON, g_keysFrench ? L"English" : L"Fran\u00E7ais");
    env->addButton(irr::core::rect<irr::s32>(w - 210, h - 44, w - 10, h - 12), g_keysWindow,
        KEYS_CLOSE_BUTTON, g_keysFrench ? L"Fermer" : L"Close");
}

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

                //KYARA TOUCHES: handled here, ABOVE the fork() below - these three only open and
                //close a window in this process, they launch nothing.
                if (id == KEYS_BUTTON) {
                    showKeyHelp();
                    return true;
                }
                if (id == KEYS_LANG_BUTTON) {
                    g_keysFrench = !g_keysFrench;
                    showKeyHelp();
                    return true;
                }
                if (id == KEYS_CLOSE_BUTTON) {
                    if (g_keysWindow) { g_keysWindow->remove(); g_keysWindow = 0; }
                    return true;
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
                if (id == FE_BUTTON) {
#ifdef _WIN32
                    ShellExecute(NULL, NULL, "Simulator-fe.exe", NULL, NULL, SW_SHOW);
#else
#ifdef __APPLE__
                    //APPLE
                    execl("../MacOS/fe.app/Contents/MacOS/fe", "fe", NULL);
#else
                    //Other (assumed posix)
                    execl("./Simulator-fe", "Simulator-fe", NULL);
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
                //KYARA TOUCHES: escape closes the sheet first, so it cannot shut the launcher
                //down by surprise while the sheet is open.
                if (g_keysWindow) {
                    g_keysWindow->remove();
                    g_keysWindow = 0;
                    return true;
                }
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
    g_device = device; //KYARA TOUCHES


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
        size_t end = s.find_last_not_of(L" \t\r\n");
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

    int c1Y = startY;
    int c2rY = startY;

    // Left Column — 4 launcher buttons
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c1, c1Y, c1 + btnW, c1Y + btnH), 0, BC_BUTTON, T("startBC").c_str()); c1Y += btnH;
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c1, c1Y, c1 + btnW, c1Y + btnH), 0, ED_BUTTON, T("startED").c_str()); c1Y += btnH;
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c1, c1Y, c1 + btnW, c1Y + btnH), 0, FE_BUTTON, T("startFE").c_str()); c1Y += btnH;
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c1, c1Y, c1 + btnW, c1Y + btnH), 0, MH_BUTTON, T("startMH").c_str());

    // Right Column — 3 settings buttons
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c2r, c2rY, c2r + btnW, c2rY + btnH), 0, INI_BC_BUTTON, T("startINIBC").c_str()); c2rY += btnH;
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c2r, c2rY, c2r + btnW, c2rY + btnH), 0, INI_MC_BUTTON, T("startINIMC").c_str()); c2rY += btnH;
    device->getGUIEnvironment()->addButton(irr::core::rect<irr::s32>(c2r, c2rY, c2r + btnW, c2rY + btnH), 0, INI_MH_BUTTON, T("startINIMH").c_str());

    //KYARA TOUCHES: the shortcut sheet, sitting just above the exit button
    {
        int keysW = 200;
        int keysH = 35;
        int keysX = (int)(graphicsWidth - keysW) / 2;
        int keysY = (int)graphicsHeight - keysH - 80;
        device->getGUIEnvironment()->addButton(
            irr::core::rect<irr::s32>(keysX, keysY, keysX + keysW, keysY + keysH), 0, KEYS_BUTTON,
            L"Raccourcis clavier", L"Liste des touches du simulateur (FR / EN)");
    }

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