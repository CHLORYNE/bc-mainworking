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
#include "../GUIPanelDraw.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <functional>

//headers for execl
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <direct.h> //_mkdir
#else
#include <unistd.h>
#include <sys/stat.h> //mkdir
#include <sys/wait.h> //waitpid
#endif

//Mac OS:
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

//Linux: the screen size from X11 when Irrlicht does not know it (no XF86VidMode).
#if !defined(_WIN32) && !defined(__APPLE__)
extern "C" {
    struct _XDisplay;
    int XDefaultScreen(struct _XDisplay*);
    int XDisplayWidth(struct _XDisplay*, int);
    int XDisplayHeight(struct _XDisplay*, int);
}
#endif

#include "../ScreenChooser.hpp" //screens of the desk, and which already show the simulator
#include "LauncherDraw.hpp"
#include "HudMenu.hpp"
#include "UiSound.hpp"
#include "VideoClip.hpp"

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

//=================================================================================================
//The launcher's screen is the full-screen menu (HudMenu.cpp), after the intro film if there is one.
//The keyboard-shortcut sheet and the screen picker below are drawn over it.
//=================================================================================================

HudMenu* g_hud = 0;

//Feedback while an application starts (it can take a few seconds).
void showLaunchToast(const std::wstring& title)
{
    if (g_hud) { g_hud->showToast(title); }
}

std::wstring g_simulatorTitle; //the simulator card's title, for the toast once a screen has been picked

//=================================================================================================
//Overlays drawn over the tiles: the keyboard-shortcut sheet and the screen picker. Each is a
//full-window GUI element, custom drawn like the tiles, and modal: while open it holds the focus and
//takes every mouse and key event. Escape, or a click outside its panel, closes it.
//=================================================================================================

namespace Overlay {
    const irr::video::SColor dim(178, 2, 8, 18);
    const irr::video::SColor panelTop(252, 17, 34, 60);
    const irr::video::SColor panelBottom(252, 9, 20, 38);
    const irr::video::SColor panelEdge(110, 90, 150, 220);
    const irr::video::SColor rule(60, 140, 180, 230);
    const irr::video::SColor hoverRow(34, 120, 180, 255);
}

irr::core::rect<irr::s32> toIntRect(const irr::core::rect<irr::f32>& r)
{
    return irr::core::rect<irr::s32>((irr::s32)r.UpperLeftCorner.X, (irr::s32)r.UpperLeftCorner.Y,
        (irr::s32)r.LowerRightCorner.X, (irr::s32)r.LowerRightCorner.Y);
}

//Text in a box: horizontally left or centred, vertically centred.
void drawTextIn(irr::gui::IGUIFont* font, const std::wstring& text, const irr::core::rect<irr::f32>& box, irr::video::SColor col,
    bool centred, const irr::core::rect<irr::s32>* clip = 0)
{
    if (!font || text.empty()) { return; }
    const irr::core::dimension2du d = font->getDimension(text.c_str());
    const irr::s32 x = centred ? (irr::s32)(box.getCenter().X - d.Width * 0.5f) : (irr::s32)box.UpperLeftCorner.X;
    const irr::s32 y = (irr::s32)(box.getCenter().Y - d.Height * 0.5f);
    font->draw(text.c_str(), irr::core::rect<irr::s32>(x, y, x + (irr::s32)d.Width + 2, y + (irr::s32)d.Height), col, false, false, clip);
}

irr::f32 textWidth(irr::gui::IGUIFont* font, const std::wstring& text)
{
    return font ? (irr::f32)font->getDimension(text.c_str()).Width : 8.0f * text.size();
}

irr::f32 textHeight(irr::gui::IGUIFont* font)
{
    return font ? (irr::f32)font->getDimension(L"Ag").Height : 14.0f;
}

//Pill button: primary (blue gradient) or secondary (glass with an outline).
void drawPillButton(irr::gui::PanelBatch& b, const irr::core::rect<irr::f32>& r, bool primary, bool hovered, bool enabled = true)
{
    const irr::f32 rad = r.getHeight() * 0.5f;
    if (primary) {
        irr::video::SColor top = hovered ? irr::video::SColor(255, 66, 150, 240) : Theme::primaryTop;
        irr::video::SColor bottom = hovered ? irr::video::SColor(255, 26, 98, 196) : Theme::primaryBottom;
        if (!enabled) { top = irr::video::SColor(255, 44, 62, 86); bottom = irr::video::SColor(255, 34, 48, 68); }
        roundRect(b, r, rad, top, bottom);
        roundRectOutline(b, r, rad, 1.0f, irr::video::SColor(enabled ? 110 : 40, 190, 225, 255));
    }
    else {
        roundRect(b, r, rad, hovered ? irr::video::SColor(220, 36, 58, 88) : irr::video::SColor(200, 24, 40, 64),
            hovered ? irr::video::SColor(220, 28, 46, 72) : irr::video::SColor(200, 18, 30, 50));
        roundRectOutline(b, r, rad, 1.0f, hovered ? irr::video::SColor(200, 120, 190, 255) : Theme::border);
    }
}

class LauncherOverlay : public irr::gui::IGUIElement
{
public:
    LauncherOverlay(irr::gui::IGUIEnvironment* env)
        : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, env, env->getRootGUIElement(), -1, irr::core::rect<irr::s32>(0, 0, 10, 10)),
        mouse(-1, -1)
    {
        setVisible(false);
    }

    void show()
    {
        fitWindow();
        setVisible(true);
        Parent->bringToFront(this);
        Environment->setFocus(this);
    }

    void hide()
    {
        setVisible(false);
        Environment->removeFocus(this);
    }

    virtual bool OnEvent(const irr::SEvent& event)
    {
        if (!IsVisible) { return IGUIElement::OnEvent(event); }
        if (event.EventType == irr::EET_GUI_EVENT) {
            //Keep the focus while open, so the tiles underneath get no keys.
            return event.GUIEvent.EventType == irr::gui::EGET_ELEMENT_FOCUS_LOST && event.GUIEvent.Caller == this;
        }
        if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
            mouse = irr::core::position2di(event.MouseInput.X, event.MouseInput.Y);
            onMouse(event.MouseInput);
            return true;
        }
        if (event.EventType == irr::EET_KEY_INPUT_EVENT) {
            onKey(event.KeyInput);
            return true;
        }
        return true;
    }

protected:
    virtual void onMouse(const irr::SEvent::SMouseInput& m) = 0;
    virtual void onKey(const irr::SEvent::SKeyInput& k) = 0;

    //Follows the window size; true if it changed.
    bool fitWindow()
    {
        const irr::core::dimension2du s = Environment->getVideoDriver()->getScreenSize();
        const irr::core::rect<irr::s32> full(0, 0, (irr::s32)s.Width, (irr::s32)s.Height);
        if (RelativeRect == full) { return false; }
        setRelativePosition(full);
        return true;
    }

    bool over(const irr::core::rect<irr::f32>& r) const
    {
        return r.isPointInside(irr::core::vector2df((irr::f32)mouse.X, (irr::f32)mouse.Y));
    }

    //Dimmed launcher and the panel body with its shadow.
    void drawPanel(irr::gui::PanelBatch& b, const irr::core::rect<irr::f32>& panel)
    {
        b.rect(irr::core::rect<irr::f32>(0, 0, (irr::f32)AbsoluteRect.getWidth(), (irr::f32)AbsoluteRect.getHeight()), Overlay::dim);
        irr::core::rect<irr::f32> s = panel;
        s.UpperLeftCorner += irr::core::vector2df(-2, 10);
        s.LowerRightCorner += irr::core::vector2df(2, 14);
        roundRect(b, s, 20, irr::video::SColor(110, 0, 0, 0), irr::video::SColor(110, 0, 0, 0));
        roundRect(b, panel, 18, Overlay::panelTop, Overlay::panelBottom);
    }

    irr::core::position2di mouse;
};

//-------------------------------------------------------------------------------------------------
//Screen picker
//-------------------------------------------------------------------------------------------------

struct ScreenInfo {
    irr::core::rect<irr::s32> area; //desktop coordinates
    bool primary;
    bool launcherHere;              //the launcher window is on this screen
    std::wstring inUse;             //the simulator's windows already open there (simulator, instruments, repeater)
};

bool g_french = true; //launcher language

//The screens, in the order the simulator numbers them (both use EnumDisplayMonitors), so screen N
//here is screen N for "-monitor N" and for bc5.ini monitor=N.
std::vector<ScreenInfo> listScreens()
{
    std::vector<ScreenInfo> screens;
#ifdef _WIN32
    const std::vector<ScreenChooser::Screen> found = ScreenChooser::listScreens(g_french);
    RECT launcherScreen = { 0, 0, 0, 0 };
    if (g_device) {
        HWND hwnd = reinterpret_cast<HWND>(g_device->getVideoDriver()->getExposedVideoData().OpenGLWin32.HWnd);
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) { launcherScreen = mi.rcMonitor; }
    }
    for (size_t i = 0; i < found.size(); i++) {
        ScreenInfo s;
        s.area = irr::core::rect<irr::s32>(found[i].left, found[i].top, found[i].right, found[i].bottom);
        s.primary = found[i].primary;
        s.launcherHere = (found[i].left == launcherScreen.left && found[i].top == launcherScreen.top
            && found[i].right == launcherScreen.right && found[i].bottom == launcherScreen.bottom);
        s.inUse = found[i].inUse;
        screens.push_back(s);
    }
#else
#ifdef LAUNCHER_TEST_SCREENS
    //Test builds only: a made-up desk (instructor screen and two large displays, a simulator already
    //open on the second), so the picker can be checked on a machine without them.
    ScreenInfo s;
    s.area = irr::core::rect<irr::s32>(0, 0, 1920, 1080); s.primary = true; s.launcherHere = true; screens.push_back(s);
    s.area = irr::core::rect<irr::s32>(1920, -180, 5760, 1980); s.primary = false; s.launcherHere = false;
    s.inUse = L"Simulateur, Instruments"; screens.push_back(s);
    s.area = irr::core::rect<irr::s32>(-1280, 56, 0, 1080); s.inUse = L""; screens.push_back(s);
#endif
#endif
    return screens;
}

class ScreenPicker : public LauncherOverlay
{
public:
    //Bridge view screen (1..n); instrument console screen (1..n, or 0: in the bridge view).
    typedef std::function<void(int, int)> LaunchFn;
    //Gyro repeater screen (1..n).
    typedef std::function<void(int)> RepeaterFn;

    ScreenPicker(irr::gui::IGUIEnvironment* env, bool french, irr::gui::IGUIFont* bigFont, irr::gui::IGUIFont* titleFont,
        irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* smallFont, LaunchFn launch, RepeaterFn launchRepeater)
        : LauncherOverlay(env), french(french), bigFont(bigFont), titleFont(titleFont), textFont(textFont), smallFont(smallFont),
        launch(launch), launchRepeater(launchRepeater), repeaterMode(false), selected(0), consoleScreen(-1), lastClickMs(0), lastClickScreen(-1)
    {
    }

    //The gyro repeater: a single screen to choose. lastScreen: the last choice (1..n, 0 for none).
    void openForRepeater(const std::vector<ScreenInfo>& list, int lastScreen)
    {
        repeaterMode = true;
        screens = list;
        const int n = (int)screens.size();
        selected = (lastScreen >= 1 && lastScreen <= n) ? lastScreen - 1 : firstOffLauncher();
        //Something already open there: offer a free screen.
        if (n > 0 && !screens[selected].inUse.empty()) {
            const int f = freeScreen();
            if (f >= 0) { selected = f; }
        }
        consoleScreen = -1;
        lastClickScreen = -1;
        show();
    }

    //lastBridge, lastConsole: the last choice (1..n, 0 for none), offered again if it fits this desk.
    void open(const std::vector<ScreenInfo>& list, int lastBridge, int lastConsole)
    {
        repeaterMode = false;
        screens = list;
        const int n = (int)screens.size();
        selected = (lastBridge >= 1 && lastBridge <= n) ? lastBridge - 1 : firstOffLauncher();
        consoleScreen = (lastConsole >= 1 && lastConsole <= n) ? lastConsole - 1 : -1;
        //Something already open there (another simulator for a second view, the repeater...): offer a free screen.
        if (n > 0 && !screens[selected].inUse.empty()) {
            const int f = freeScreen();
            if (f >= 0) { selected = f; }
        }
        if (consoleScreen == selected || (consoleScreen >= 0 && !screens[consoleScreen].inUse.empty())) { consoleScreen = -1; }
        lastClickScreen = -1;
        show();
    }

    virtual void draw()
    {
        if (!IsVisible) { return; }
        fitWindow();
        layout();
        irr::video::IVideoDriver* driver = Environment->getVideoDriver();

        irr::gui::PanelBatch b;
        b.begin(driver);
        drawPanel(b, panel);
        //Screens: bridge view in blue, instruments in teal.
        for (size_t i = 0; i < screens.size(); i++) {
            const irr::core::rect<irr::f32>& r = screenRects[i];
            const bool bridge = ((int)i == selected);
            const bool instruments = ((int)i == consoleScreen);
            const bool hov = over(r);
            irr::core::rect<irr::f32> s = r;
            s.UpperLeftCorner.Y += 5; s.LowerRightCorner.Y += 5;
            roundRect(b, s, 10, irr::video::SColor(90, 0, 0, 0), irr::video::SColor(90, 0, 0, 0));
            if (bridge || instruments) {
                const irr::video::SColor glowCol = bridge ? irr::video::SColor(70, 120, 190, 255) : irr::video::SColor(70, 90, 220, 200);
                irr::core::rect<irr::f32> glow = r;
                glow.UpperLeftCorner -= irr::core::vector2df(4, 4);
                glow.LowerRightCorner += irr::core::vector2df(4, 4);
                roundRectOutline(b, glow, 14, 3.0f, glowCol);
                if (bridge) { roundRect(b, r, 10, Theme::primaryTop, Theme::primaryBottom); }
                else { roundRect(b, r, 10, consoleTop, consoleBottom); }
                roundRectOutline(b, r, 10, 1.0f, bridge ? irr::video::SColor(255, 200, 230, 255) : irr::video::SColor(255, 170, 245, 232));
            }
            else {
                roundRect(b, r, 10, hov ? irr::video::SColor(255, 46, 70, 104) : irr::video::SColor(255, 32, 50, 76),
                    hov ? irr::video::SColor(255, 30, 48, 74) : irr::video::SColor(255, 21, 34, 54));
                roundRectOutline(b, r, 10, 1.0f, hov ? irr::video::SColor(230, 120, 190, 255) : irr::video::SColor(90, 150, 190, 240));
            }
        }
        //Instruments: in the bridge view, or on one of the other screens.
        for (size_t c = 0; c < chips.size(); c++) {
            const bool on = (chips[c].screen == consoleScreen);
            const bool hov = over(chips[c].area);
            const irr::f32 rad = chips[c].area.getHeight() * 0.5f;
            if (on) {
                roundRect(b, chips[c].area, rad, consoleTop, consoleBottom);
                roundRectOutline(b, chips[c].area, rad, 1.0f, irr::video::SColor(255, 170, 245, 232));
            }
            else {
                roundRect(b, chips[c].area, rad, hov ? irr::video::SColor(220, 36, 58, 88) : irr::video::SColor(200, 20, 34, 56),
                    hov ? irr::video::SColor(220, 28, 46, 72) : irr::video::SColor(200, 16, 28, 46));
                roundRectOutline(b, chips[c].area, rad, 1.0f, hov ? irr::video::SColor(220, 140, 230, 215) : Theme::border);
            }
        }
        drawPillButton(b, cancelButton, false, over(cancelButton));
        drawPillButton(b, launchButton, true, over(launchButton));
        b.flush();

        //Text
        const irr::f32 x = panel.UpperLeftCorner.X + 36;
        drawTextIn(titleFont, repeaterMode ? (french ? L"Choisir l'\u00E9cran du r\u00E9p\u00E9titeur gyro" : L"Choose the gyro repeater screen")
            : (french ? L"Choisir les \u00E9crans du simulateur" : L"Choose the simulator screens"),
            irr::core::rect<irr::f32>(x, panel.UpperLeftCorner.Y + 22, panel.LowerRightCorner.X, panel.UpperLeftCorner.Y + 52), Theme::text, false);
        drawTextIn(textFont, repeaterMode ? (french ? L"Cliquez sur l'\u00E9cran o\u00F9 afficher le r\u00E9p\u00E9titeur." : L"Click the screen the repeater should be shown on.")
            : (french ? L"Cliquez sur l'\u00E9cran de la vue passerelle. Les instruments peuvent aller sur un autre \u00E9cran."
            : L"Click the screen for the bridge view. The instruments can go on another screen."),
            irr::core::rect<irr::f32>(x, panel.UpperLeftCorner.Y + 54, panel.LowerRightCorner.X, panel.UpperLeftCorner.Y + 78), Theme::textDim, false);

        for (size_t i = 0; i < screens.size(); i++) {
            const irr::core::rect<irr::f32>& r = screenRects[i];
            const bool bridge = ((int)i == selected);
            const bool instruments = ((int)i == consoleScreen);
            const irr::video::SColor main = (bridge || instruments) ? irr::video::SColor(255, 255, 255, 255) : Theme::text;
            const irr::video::SColor sub = bridge ? irr::video::SColor(255, 214, 232, 252)
                : (instruments ? irr::video::SColor(255, 206, 246, 238) : Theme::textDim);
            const irr::core::rect<irr::s32> clip = toIntRect(r);
            //Number, role, resolution, tags, then what is already open there, centred as a block.
            struct Line { std::wstring text; irr::gui::IGUIFont* font; irr::video::SColor colour; };
            std::vector<Line> lines;
            Line number = { std::to_wstring(i + 1), bigFont, main };
            lines.push_back(number);
            if (bridge) {
                Line role = { repeaterMode ? (french ? L"R\u00E9p\u00E9titeur gyro" : L"Gyro repeater") : (french ? L"Vue passerelle" : L"Bridge view"), textFont, sub };
                lines.push_back(role);
            }
            if (instruments) { Line role = { L"Instruments", textFont, sub }; lines.push_back(role); }
            Line size = { std::to_wstring(screens[i].area.getWidth()) + L" \u00D7 " + std::to_wstring(screens[i].area.getHeight()), smallFont, sub };
            lines.push_back(size);
            std::wstring tags;
            if (screens[i].primary) { tags += french ? L"Principal" : L"Primary"; }
            if (screens[i].launcherHere) { tags += tags.empty() ? L"" : L"  \u00B7  "; tags += french ? L"Lanceur ici" : L"Launcher here"; }
            if (!tags.empty()) { Line tagLine = { tags, smallFont, sub }; lines.push_back(tagLine); }
            if (!screens[i].inUse.empty()) {
                Line used = { (french ? L"Ouvert : " : L"Open: ") + screens[i].inUse, smallFont, inUseColour };
                lines.push_back(used);
            }
            irr::f32 total = 0;
            for (size_t l = 0; l < lines.size(); l++) { total += textHeight(lines[l].font) + 2; }
            while (lines.size() > 1 && total > r.getHeight() - 6) { //small tile: what is open there matters more than the tags
                const size_t drop = (lines.size() > 2 && !screens[i].inUse.empty()) ? lines.size() - 2 : lines.size() - 1;
                total -= textHeight(lines[drop].font) + 2;
                lines.erase(lines.begin() + drop);
            }
            irr::f32 y = r.getCenter().Y - total * 0.5f;
            for (size_t l = 0; l < lines.size(); l++) {
                const irr::f32 h = textHeight(lines[l].font);
                drawTextIn(lines[l].font, lines[l].text, irr::core::rect<irr::f32>(r.UpperLeftCorner.X, y, r.LowerRightCorner.X, y + h),
                    lines[l].colour, true, &clip);
                y += h + 2;
            }
        }

        if (!repeaterMode) { drawTextIn(textFont, french ? L"Instruments :" : L"Instruments:", instrumentsLabel, Theme::text, false); }
        for (size_t c = 0; c < chips.size(); c++) {
            const bool on = (chips[c].screen == consoleScreen);
            drawTextIn(textFont, chips[c].label, chips[c].area, on ? irr::video::SColor(255, 255, 255, 255) : Theme::text, true);
        }
        drawTextIn(smallFont, french ? L"Le choix est propos\u00E9 \u00E0 nouveau au prochain lancement. Les fen\u00EAtres d\u00E9j\u00E0 ouvertes sont indiqu\u00E9es sur les \u00E9crans."
            : L"The choice is offered again next time. Windows already open are shown on the screens.",
            infoRow, Theme::textDim, false);

        //Key hints: as many as fit before the buttons.
        const wchar_t* hintsFr[4] = { L"1 \u00E0 9 : vue passerelle", L"I : instruments", L"Entr\u00E9e : lancer", L"\u00C9chap : annuler" };
        const wchar_t* hintsEn[4] = { L"1 to 9: bridge view", L"I: instruments", L"Enter: launch", L"Esc: cancel" };
        if (repeaterMode) {
            hintsFr[0] = L"1 \u00E0 9 : \u00E9cran"; hintsFr[1] = L"Entr\u00E9e : lancer"; hintsFr[2] = L"\u00C9chap : annuler"; hintsFr[3] = L"";
            hintsEn[0] = L"1 to 9: screen"; hintsEn[1] = L"Enter: launch"; hintsEn[2] = L"Esc: cancel"; hintsEn[3] = L"";
        }
        std::wstring hints;
        for (int h = 0; h < 4; h++) {
            const std::wstring part = french ? hintsFr[h] : hintsEn[h];
            if (part.empty()) { break; }
            const std::wstring longer = hints.empty() ? part : hints + L"  \u00B7  " + part;
            if (textWidth(smallFont, longer) > cancelButton.UpperLeftCorner.X - 16 - x) { break; }
            hints = longer;
        }
        drawTextIn(smallFont, hints, irr::core::rect<irr::f32>(x, launchButton.UpperLeftCorner.Y, cancelButton.UpperLeftCorner.X - 12, launchButton.LowerRightCorner.Y),
            irr::video::SColor(170, 178, 192, 208), false);
        drawTextIn(textFont, french ? L"Annuler" : L"Cancel", cancelButton, Theme::text, true);
        drawTextIn(textFont, repeaterMode ? (french ? L"Lancer le r\u00E9p\u00E9titeur" : L"Launch the repeater") : (french ? L"Lancer le simulateur" : L"Launch the simulator"),
            launchButton, irr::video::SColor(255, 255, 255, 255), true);
    }

protected:
    virtual void onMouse(const irr::SEvent::SMouseInput& m)
    {
        const bool left = (m.Event == irr::EMIE_LMOUSE_LEFT_UP);
        const bool right = (m.Event == irr::EMIE_RMOUSE_LEFT_UP);
        if (!left && !right) { return; }
        layout();
        for (size_t i = 0; i < screenRects.size(); i++) {
            if (over(screenRects[i])) {
                if (right) { //right click: instruments on this screen (or back in the view)
                    if (!repeaterMode && (int)i != selected) { consoleScreen = (consoleScreen == (int)i) ? -1 : (int)i; }
                    return;
                }
                //Double click on a screen launches at once.
                const irr::u32 now = g_device ? g_device->getTimer()->getRealTime() : 0;
                const bool doubleClick = (lastClickScreen == (int)i) && (now - lastClickMs < 450);
                selectBridge((int)i);
                lastClickScreen = (int)i;
                lastClickMs = now;
                if (doubleClick) { confirm(); }
                return;
            }
        }
        if (!left) { return; }
        for (size_t c = 0; c < chips.size(); c++) {
            if (over(chips[c].area)) { consoleScreen = chips[c].screen; return; }
        }
        if (over(launchButton)) { confirm(); return; }
        if (over(cancelButton) || !over(panel)) { hide(); return; }
    }

    virtual void onKey(const irr::SEvent::SKeyInput& k)
    {
        if (k.PressedDown) { return; }
        if (k.Key == irr::KEY_ESCAPE) { hide(); return; }
        if (k.Key == irr::KEY_RETURN || k.Key == irr::KEY_SPACE) { confirm(); return; }
        const int n = (int)screens.size();
        if (k.Key >= irr::KEY_KEY_1 && k.Key <= irr::KEY_KEY_9 && (int)(k.Key - irr::KEY_KEY_1) < n) { selectBridge((int)(k.Key - irr::KEY_KEY_1)); }
        if (k.Key >= irr::KEY_NUMPAD1 && k.Key <= irr::KEY_NUMPAD9 && (int)(k.Key - irr::KEY_NUMPAD1) < n) { selectBridge((int)(k.Key - irr::KEY_NUMPAD1)); }
        if (n < 1) { return; }
        if (k.Key == irr::KEY_LEFT || k.Key == irr::KEY_UP) { selectBridge((selected + n - 1) % n); }
        if (k.Key == irr::KEY_RIGHT || k.Key == irr::KEY_DOWN || k.Key == irr::KEY_TAB) { selectBridge((selected + 1) % n); }
        if (k.Key == irr::KEY_KEY_I && !repeaterMode) { //next choice for the instruments
            layout();
            for (size_t c = 0; c < chips.size(); c++) {
                if (chips[c].screen == consoleScreen) { consoleScreen = chips[(c + 1) % chips.size()].screen; break; }
            }
        }
    }

private:
    struct Chip {
        int screen; //-1: in the bridge view
        std::wstring label;
        irr::core::rect<irr::f32> area;
    };

    //The bridge view goes on screen i; if the instruments were there, they take the bridge view's old screen.
    void selectBridge(int i)
    {
        if (i == selected) { return; }
        if (i == consoleScreen) { consoleScreen = selected; }
        selected = i;
    }

    void confirm()
    {
        if (selected < 0 || selected >= (int)screens.size()) { return; }
        hide();
        if (repeaterMode) {
            if (launchRepeater) { launchRepeater(selected + 1); }
        }
        else if (launch) {
            launch(selected + 1, consoleScreen >= 0 ? consoleScreen + 1 : 0);
        }
    }

    //Most often the programs go on a screen other than the instructor's, where the launcher is.
    int firstOffLauncher() const
    {
        for (size_t i = 0; i < screens.size(); i++) {
            if (!screens[i].launcherHere) { return (int)i; }
        }
        return 0;
    }

    //A screen with none of the simulator's windows on it, preferably not the launcher's; -1 if none.
    int freeScreen() const
    {
        for (size_t i = 0; i < screens.size(); i++) {
            if (screens[i].inUse.empty() && !screens[i].launcherHere) { return (int)i; }
        }
        for (size_t i = 0; i < screens.size(); i++) {
            if (screens[i].inUse.empty()) { return (int)i; }
        }
        return -1;
    }

    void layout()
    {
        const irr::f32 W = (irr::f32)AbsoluteRect.getWidth(), H = (irr::f32)AbsoluteRect.getHeight();
        const irr::f32 pw = irr::core::min_(860.0f, W - 64), ph = irr::core::min_(580.0f, H - 48);
        panel = irr::core::rect<irr::f32>((W - pw) * 0.5f, (H - ph) * 0.5f, (W + pw) * 0.5f, (H + ph) * 0.5f);
        const irr::f32 left = panel.UpperLeftCorner.X + 36, right = panel.LowerRightCorner.X - 36;
        const irr::f32 bottom = panel.LowerRightCorner.Y;

        launchButton = irr::core::rect<irr::f32>(right - 250, bottom - 70, right, bottom - 26);
        cancelButton = irr::core::rect<irr::f32>(launchButton.UpperLeftCorner.X - 142, bottom - 70, launchButton.UpperLeftCorner.X - 12, bottom - 26);
        infoRow = irr::core::rect<irr::f32>(left, bottom - 116, right, bottom - 94);

        //Instruments row (simulator only): a chip for "in the bridge view", then one per other screen.
        const irr::f32 chipTop = repeaterMode ? infoRow.UpperLeftCorner.Y : bottom - 168, chipBottom = bottom - 134;
        instrumentsLabel = irr::core::rect<irr::f32>(left, chipTop, left + textWidth(textFont, french ? L"Instruments :" : L"Instruments:") + 8, chipBottom);
        chips.clear();
        if (!repeaterMode) {
            Chip inView;
            inView.screen = -1;
            inView.label = french ? L"Dans la vue passerelle" : L"In the bridge view";
            chips.push_back(inView);
            for (size_t i = 0; i < screens.size(); i++) {
                if ((int)i == selected) { continue; }
                Chip c;
                c.screen = (int)i;
                c.label = (french ? L"\u00C9cran " : L"Screen ") + std::to_wstring(i + 1);
                chips.push_back(c);
            }
            irr::f32 cx = instrumentsLabel.LowerRightCorner.X + 6;
            for (size_t c = 0; c < chips.size(); c++) {
                const irr::f32 w = textWidth(textFont, chips[c].label) + 34;
                if (c > 0 && cx + w > right) { chips.resize(c); break; } //no room for more
                chips[c].area = irr::core::rect<irr::f32>(cx, chipTop, cx + w, chipBottom);
                cx += w + 8;
            }
        }

        //Desk drawn to scale in the space between the title and the instruments row (or the note below).
        const irr::core::rect<irr::f32> area(left, panel.UpperLeftCorner.Y + 100, right, chipTop - 22);
        screenRects.clear();
        if (screens.empty()) { return; }
        irr::core::rect<irr::s32> desk = screens[0].area;
        for (size_t i = 1; i < screens.size(); i++) { desk.addInternalPoint(screens[i].area.UpperLeftCorner); desk.addInternalPoint(screens[i].area.LowerRightCorner); }
        const irr::f32 dw = (irr::f32)irr::core::max_(1, desk.getWidth()), dh = (irr::f32)irr::core::max_(1, desk.getHeight());
        const irr::f32 scale = irr::core::min_(area.getWidth() / dw, area.getHeight() / dh) * 0.94f;
        const irr::f32 ox = area.getCenter().X - dw * scale * 0.5f, oy = area.getCenter().Y - dh * scale * 0.5f;
        for (size_t i = 0; i < screens.size(); i++) {
            const irr::core::rect<irr::s32>& a = screens[i].area;
            irr::core::rect<irr::f32> r(ox + (a.UpperLeftCorner.X - desk.UpperLeftCorner.X) * scale, oy + (a.UpperLeftCorner.Y - desk.UpperLeftCorner.Y) * scale,
                ox + (a.LowerRightCorner.X - desk.UpperLeftCorner.X) * scale, oy + (a.LowerRightCorner.Y - desk.UpperLeftCorner.Y) * scale);
            r.UpperLeftCorner += irr::core::vector2df(5, 5); //gap between neighbouring screens
            r.LowerRightCorner -= irr::core::vector2df(5, 5);
            screenRects.push_back(r);
        }
    }

    const irr::video::SColor consoleTop = irr::video::SColor(255, 26, 132, 128);
    const irr::video::SColor consoleBottom = irr::video::SColor(255, 12, 88, 90);
    const irr::video::SColor inUseColour = irr::video::SColor(255, 255, 200, 100);

    bool french;
    irr::gui::IGUIFont* bigFont;
    irr::gui::IGUIFont* titleFont;
    irr::gui::IGUIFont* textFont;
    irr::gui::IGUIFont* smallFont;
    LaunchFn launch;
    RepeaterFn launchRepeater;
    bool repeaterMode;     //choosing the gyro repeater's screen, not the simulator's
    std::vector<ScreenInfo> screens;
    std::vector<irr::core::rect<irr::f32> > screenRects;
    std::vector<Chip> chips;
    irr::core::rect<irr::f32> panel, launchButton, cancelButton, infoRow, instrumentsLabel;
    int selected;          //bridge view screen (index)
    int consoleScreen;     //instruments screen (index), -1: in the bridge view
    irr::u32 lastClickMs;
    int lastClickScreen;
};

//-------------------------------------------------------------------------------------------------
//Keyboard-shortcut sheet: the KEY_ROWS table as sections in columns, with drawn key caps.
//-------------------------------------------------------------------------------------------------

class KeySheet : public LauncherOverlay
{
public:
    KeySheet(irr::gui::IGUIEnvironment* env, irr::gui::IGUIFont* titleFont, irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* smallFont)
        : LauncherOverlay(env), titleFont(titleFont), textFont(textFont), smallFont(smallFont), columnWidth(0), keyColumn(0), scroll(0), maxScroll(0),
        builtFrench(false), builtWidth(0), builtHeight(0)
    {
    }

    void open()
    {
        scroll = 0;
        builtWidth = 0; //rebuild
        show();
    }

    virtual void draw()
    {
        if (!IsVisible) { return; }
        fitWindow();
        if (builtWidth != AbsoluteRect.getWidth() || builtHeight != AbsoluteRect.getHeight() || builtFrench != g_keysFrench) { build(); }
        irr::video::IVideoDriver* driver = Environment->getVideoDriver();
        const irr::core::rect<irr::s32> bodyClip = toIntRect(body);
        const irr::f32 dy = body.UpperLeftCorner.Y - scroll;
        const irr::f32 capH = textHeight(smallFont) + 10;

        irr::gui::PanelBatch b;
        b.begin(driver);
        drawPanel(b, panel);
        //Rows: hover band, then key caps.
        for (size_t i = 0; i < rows.size(); i++) {
            const Row& r = rows[i];
            const irr::f32 y = r.y + dy;
            if (y + r.h < body.UpperLeftCorner.Y || y > body.LowerRightCorner.Y) { continue; }
            const irr::core::rect<irr::f32> band(r.x - 8, y - 3, r.x + columnWidth + 8, y + r.h - 5);
            if (over(band) && over(body)) { roundRect(b, band, 8, Overlay::hoverRow, Overlay::hoverRow); }
            irr::f32 cx = r.x, cy = y;
            for (size_t c = 0; c < r.caps.size(); c++) {
                const Cap& cap = r.caps[c];
                if (cx + cap.w > r.x + keyColumn && cx > r.x) { cx = r.x; cy += capH + 6; } //wrap a long combination
                if (cap.key) {
                    const irr::core::rect<irr::f32> k(cx, cy, cx + cap.w, cy + capH);
                    irr::core::rect<irr::f32> lip = k;
                    lip.UpperLeftCorner.Y += 2; lip.LowerRightCorner.Y += 3;
                    roundRect(b, lip, 6, irr::video::SColor(255, 6, 12, 22), irr::video::SColor(255, 6, 12, 22));
                    roundRect(b, k, 6, irr::video::SColor(255, 64, 88, 122), irr::video::SColor(255, 38, 56, 82));
                    roundRectOutline(b, k, 6, 1.0f, irr::video::SColor(120, 170, 205, 245));
                }
                cx += cap.w + 6;
            }
        }
        //Section headings: accent bar and rule.
        for (size_t i = 0; i < heads.size(); i++) {
            const Head& h = heads[i];
            const irr::f32 y = h.y + dy;
            if (y + 30 < body.UpperLeftCorner.Y || y > body.LowerRightCorner.Y) { continue; }
            b.rect(irr::core::rect<irr::f32>(h.x, y + 6, h.x + 4, y + 24), accent(h.section));
            b.rect(irr::core::rect<irr::f32>(h.x, y + 32, h.x + columnWidth, y + 33), Overlay::rule);
        }
        //Header and footer bands over the scrolled content, then their controls.
        roundRect(b, irr::core::rect<irr::f32>(panel.UpperLeftCorner.X, panel.UpperLeftCorner.Y, panel.LowerRightCorner.X, body.UpperLeftCorner.Y), 18,
            Overlay::panelTop, irr::video::SColor(252, 15, 31, 56));
        b.rect(irr::core::rect<irr::f32>(panel.UpperLeftCorner.X, body.UpperLeftCorner.Y - 18, panel.LowerRightCorner.X, body.UpperLeftCorner.Y), irr::video::SColor(252, 15, 31, 56));
        b.rect(irr::core::rect<irr::f32>(panel.UpperLeftCorner.X + 28, body.UpperLeftCorner.Y - 1, panel.LowerRightCorner.X - 28, body.UpperLeftCorner.Y), Overlay::rule);
        roundRect(b, irr::core::rect<irr::f32>(panel.UpperLeftCorner.X, body.LowerRightCorner.Y, panel.LowerRightCorner.X, panel.LowerRightCorner.Y), 18,
            irr::video::SColor(252, 10, 21, 40), Overlay::panelBottom);
        b.rect(irr::core::rect<irr::f32>(panel.UpperLeftCorner.X, body.LowerRightCorner.Y, panel.LowerRightCorner.X, body.LowerRightCorner.Y + 18), irr::video::SColor(252, 10, 21, 40));
        b.rect(irr::core::rect<irr::f32>(panel.UpperLeftCorner.X + 28, body.LowerRightCorner.Y, panel.LowerRightCorner.X - 28, body.LowerRightCorner.Y + 1), Overlay::rule);
        roundRectOutline(b, panel, 18, 1.0f, Overlay::panelEdge);
        //Keyboard badge
        b.disc(badgeCentre, 24, irr::video::SColor(70, 64, 156, 240), irr::video::SColor(70, 64, 156, 240));
        drawIcon(b, Icon_Keys, badgeCentre, 14.0f, Theme::accentHi);
        //Language switch: two segments, the active one filled.
        roundRect(b, langSwitch, langSwitch.getHeight() * 0.5f, irr::video::SColor(255, 12, 24, 42), irr::video::SColor(255, 12, 24, 42));
        const irr::core::rect<irr::f32> active = g_keysFrench ? langFr : langEn;
        roundRect(b, active, active.getHeight() * 0.5f, Theme::primaryTop, Theme::primaryBottom);
        roundRectOutline(b, langSwitch, langSwitch.getHeight() * 0.5f, 1.0f, Theme::border);
        //Close cross
        if (over(closeButton)) { b.disc(closeButton.getCenter(), closeButton.getWidth() * 0.5f, irr::video::SColor(90, 214, 72, 72), irr::video::SColor(90, 214, 72, 72)); }
        {
            const irr::core::vector2df c = closeButton.getCenter();
            const irr::f32 s = 7;
            b.line(irr::core::vector2df(c.X - s, c.Y - s), irr::core::vector2df(c.X + s, c.Y + s), 2.0f, Theme::text);
            b.line(irr::core::vector2df(c.X - s, c.Y + s), irr::core::vector2df(c.X + s, c.Y - s), 2.0f, Theme::text);
        }
        drawPillButton(b, doneButton, true, over(doneButton));
        //Scroll bar, when the content is taller than the body.
        if (maxScroll > 0) {
            const irr::f32 trackX = panel.LowerRightCorner.X - 14;
            const irr::f32 th = body.getHeight() * body.getHeight() / (body.getHeight() + maxScroll);
            const irr::f32 ty = body.UpperLeftCorner.Y + (body.getHeight() - th) * (scroll / maxScroll);
            roundRect(b, irr::core::rect<irr::f32>(trackX, body.UpperLeftCorner.Y, trackX + 4, body.LowerRightCorner.Y), 2,
                irr::video::SColor(50, 150, 190, 240), irr::video::SColor(50, 150, 190, 240));
            roundRect(b, irr::core::rect<irr::f32>(trackX - 1, ty, trackX + 5, ty + th), 3, irr::video::SColor(200, 120, 180, 245), irr::video::SColor(200, 90, 150, 225));
        }
        b.flush();

        //Text: rows and headings (clipped to the body), then the header and footer.
        for (size_t i = 0; i < heads.size(); i++) {
            const Head& h = heads[i];
            const irr::f32 y = h.y + dy;
            if (y + 30 < body.UpperLeftCorner.Y || y > body.LowerRightCorner.Y) { continue; }
            drawTextIn(textFont, h.title, irr::core::rect<irr::f32>(h.x + 14, y + 2, h.x + columnWidth, y + 28), accent(h.section), false, &bodyClip);
        }
        const irr::f32 lineH = textHeight(textFont);
        for (size_t i = 0; i < rows.size(); i++) {
            const Row& r = rows[i];
            const irr::f32 y = r.y + dy;
            if (y + r.h < body.UpperLeftCorner.Y || y > body.LowerRightCorner.Y) { continue; }
            irr::f32 cx = r.x, cy = y;
            for (size_t c = 0; c < r.caps.size(); c++) {
                const Cap& cap = r.caps[c];
                if (cx + cap.w > r.x + keyColumn && cx > r.x) { cx = r.x; cy += capH + 6; }
                drawTextIn(smallFont, cap.text, irr::core::rect<irr::f32>(cx, cy, cx + cap.w, cy + capH),
                    cap.key ? irr::video::SColor(255, 236, 242, 250) : irr::video::SColor(255, 128, 150, 178), true, &bodyClip);
                cx += cap.w + 6;
            }
            irr::f32 ty = y + (capH - lineH) * 0.5f;
            for (size_t l = 0; l < r.desc.size(); l++) {
                drawTextIn(textFont, r.desc[l], irr::core::rect<irr::f32>(r.x + keyColumn + 18, ty, r.x + columnWidth, ty + lineH), Theme::text, false, &bodyClip);
                ty += lineH;
            }
        }
        const bool fr = g_keysFrench;
        drawTextIn(titleFont, fr ? L"Raccourcis clavier du simulateur" : L"Simulator keyboard shortcuts",
            irr::core::rect<irr::f32>(badgeCentre.X + 38, panel.UpperLeftCorner.Y + 22, langSwitch.UpperLeftCorner.X - 12, panel.UpperLeftCorner.Y + 50), Theme::text, false);
        drawTextIn(smallFont, fr ? L"Touches utilisables pendant un exercice, dans la vue passerelle" : L"Keys available during an exercise, in the bridge view",
            irr::core::rect<irr::f32>(badgeCentre.X + 38, panel.UpperLeftCorner.Y + 50, langSwitch.UpperLeftCorner.X - 12, panel.UpperLeftCorner.Y + 72), Theme::textDim, false);
        drawTextIn(smallFont, L"FR", langFr, fr ? irr::video::SColor(255, 255, 255, 255) : Theme::textDim, true);
        drawTextIn(smallFont, L"EN", langEn, !fr ? irr::video::SColor(255, 255, 255, 255) : Theme::textDim, true);
        drawTextIn(smallFont, fr ? L"Molette : faire d\u00E9filer  \u00B7  \u00C9chap : fermer" : L"Wheel: scroll  \u00B7  Esc: close",
            irr::core::rect<irr::f32>(panel.UpperLeftCorner.X + 30, doneButton.UpperLeftCorner.Y, doneButton.UpperLeftCorner.X - 12, doneButton.LowerRightCorner.Y),
            irr::video::SColor(170, 178, 192, 208), false);
        drawTextIn(textFont, fr ? L"Fermer" : L"Close", doneButton, irr::video::SColor(255, 255, 255, 255), true);
    }

protected:
    virtual void onMouse(const irr::SEvent::SMouseInput& m)
    {
        if (m.Event == irr::EMIE_MOUSE_WHEEL) {
            scroll = irr::core::clamp(scroll - m.Wheel * 64.0f, 0.0f, maxScroll);
            return;
        }
        if (m.Event != irr::EMIE_LMOUSE_LEFT_UP) { return; }
        if (over(langFr)) { g_keysFrench = true; return; }
        if (over(langEn)) { g_keysFrench = false; return; }
        if (over(closeButton) || over(doneButton) || !over(panel)) { hide(); }
    }

    virtual void onKey(const irr::SEvent::SKeyInput& k)
    {
        if (!k.PressedDown) {
            if (k.Key == irr::KEY_ESCAPE || k.Key == irr::KEY_RETURN) { hide(); }
            return;
        }
        if (k.Key == irr::KEY_DOWN) { scroll += 40; }
        if (k.Key == irr::KEY_UP) { scroll -= 40; }
        if (k.Key == irr::KEY_NEXT) { scroll += body.getHeight() * 0.9f; }
        if (k.Key == irr::KEY_PRIOR) { scroll -= body.getHeight() * 0.9f; }
        if (k.Key == irr::KEY_HOME) { scroll = 0; }
        if (k.Key == irr::KEY_END) { scroll = maxScroll; }
        scroll = irr::core::clamp(scroll, 0.0f, maxScroll);
    }

private:
    struct Cap { std::wstring text; bool key; irr::f32 w; };
    struct Row { std::vector<Cap> caps; std::vector<std::wstring> desc; irr::f32 x, y, h; };
    struct Head { std::wstring title; irr::f32 x, y; int section; };

    static irr::video::SColor accent(int section)
    {
        static const irr::video::SColor colours[] = {
            irr::video::SColor(255, 255, 182, 72),   //engines
            irr::video::SColor(255, 255, 138, 84),   //azimuth drive
            irr::video::SColor(255, 84, 204, 255),   //view
            irr::video::SColor(255, 176, 150, 255),  //time
            irr::video::SColor(255, 255, 222, 100),  //lighting
            irr::video::SColor(255, 255, 112, 128),  //manoeuvres
            irr::video::SColor(255, 160, 178, 206) };//quit
        const int n = sizeof(colours) / sizeof(colours[0]);
        return colours[section < 0 ? 0 : section % n];
    }

    //Key names in English for the English sheet (the table holds the French names).
    static std::wstring keyName(const std::wstring& word, bool french)
    {
        if (french) { return word; }
        static const wchar_t* map[][2] = {
            { L"Maj", L"Shift" }, { L"Espace", L"Space" }, { L"Entr\u00E9e", L"Enter" }, { L"\u00C9chap", L"Esc" },
            { L"Fl\u00E8ches", L"Arrows" }, { L"Haut", L"Up" }, { L"Bas", L"Down" }, { L"Gauche", L"Left" },
            { L"Droite", L"Right" }, { L"ou", L"or" } };
        for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
            if (word == map[i][0]) { return map[i][1]; }
        }
        return word;
    }

    //"Ctrl + Maj + Haut / Bas" -> caps Ctrl, Maj, Haut, Bas with "+" and "/" between them.
    std::vector<Cap> parseKeys(const std::wstring& keys, bool french) const
    {
        std::vector<Cap> caps;
        std::wstring word;
        for (size_t i = 0; i <= keys.size(); i++) {
            if (i == keys.size() || keys[i] == L' ') {
                if (!word.empty()) {
                    Cap c;
                    c.key = !(word == L"+" || word == L"/" || word == L"ou");
                    c.text = keyName(word, french);
                    c.w = textWidth(smallFont, c.text) + (c.key ? 18.0f : 2.0f);
                    if (c.key) { c.w = irr::core::max_(c.w, textHeight(smallFont) + 10); }
                    caps.push_back(c);
                }
                word.clear();
            }
            else {
                word += keys[i];
            }
        }
        return caps;
    }

    static irr::f32 capsWidth(const std::vector<Cap>& caps)
    {
        irr::f32 w = 0;
        for (size_t i = 0; i < caps.size(); i++) { w += caps[i].w + (i ? 6.0f : 0.0f); }
        return w;
    }

    void build()
    {
        builtFrench = g_keysFrench;
        builtWidth = AbsoluteRect.getWidth();
        builtHeight = AbsoluteRect.getHeight();
        const irr::f32 W = (irr::f32)builtWidth, H = (irr::f32)builtHeight;

        const irr::f32 pw = irr::core::min_(1560.0f, W - 56), ph = H - 48;
        panel = irr::core::rect<irr::f32>((W - pw) * 0.5f, (H - ph) * 0.5f, (W + pw) * 0.5f, (H + ph) * 0.5f);
        const irr::f32 headerH = 92, footerH = 66;
        body = irr::core::rect<irr::f32>(panel.UpperLeftCorner.X + 30, panel.UpperLeftCorner.Y + headerH + 12,
            panel.LowerRightCorner.X - 30, panel.LowerRightCorner.Y - footerH - 8);
        badgeCentre = irr::core::vector2df(panel.UpperLeftCorner.X + 30 + 24, panel.UpperLeftCorner.Y + 46);
        closeButton = irr::core::rect<irr::f32>(panel.LowerRightCorner.X - 58, panel.UpperLeftCorner.Y + 28, panel.LowerRightCorner.X - 26, panel.UpperLeftCorner.Y + 60);
        langSwitch = irr::core::rect<irr::f32>(closeButton.UpperLeftCorner.X - 18 - 112, panel.UpperLeftCorner.Y + 29, closeButton.UpperLeftCorner.X - 18, panel.UpperLeftCorner.Y + 59);
        langFr = irr::core::rect<irr::f32>(langSwitch.UpperLeftCorner.X + 3, langSwitch.UpperLeftCorner.Y + 3, langSwitch.getCenter().X, langSwitch.LowerRightCorner.Y - 3);
        langEn = irr::core::rect<irr::f32>(langSwitch.getCenter().X, langSwitch.UpperLeftCorner.Y + 3, langSwitch.LowerRightCorner.X - 3, langSwitch.LowerRightCorner.Y - 3);
        doneButton = irr::core::rect<irr::f32>(panel.LowerRightCorner.X - 30 - 160, panel.LowerRightCorner.Y - footerH + 10, panel.LowerRightCorner.X - 30, panel.LowerRightCorner.Y - 14);

        //Columns: as many as fit at about 520 px each.
        const irr::f32 gap = 40;
        const int columns = irr::core::clamp((int)((body.getWidth() + gap) / (520 + gap)), 1, 3);
        columnWidth = (body.getWidth() - gap * (columns - 1) - 14) / columns; //14: room for the scroll bar

        //Key column: the widest combination, within reason.
        rows.clear();
        heads.clear();
        irr::f32 widest = 0;
        for (int i = 0; i < KEY_ROW_COUNT; i++) {
            if (KEY_ROWS[i].keys) { widest = irr::core::max_(widest, capsWidth(parseKeys(KEY_ROWS[i].keys, builtFrench))); }
        }
        keyColumn = irr::core::min_(widest, columnWidth * 0.46f);

        //Sections go, in order, into whichever column is shortest so far.
        const irr::f32 capH = textHeight(smallFont) + 10, lineH = textHeight(textFont);
        std::vector<irr::f32> columnY(columns, 0.0f);
        int section = -1;
        int column = 0;
        for (int i = 0; i < KEY_ROW_COUNT; i++) {
            if (KEY_ROWS[i].keys == 0) {
                section++;
                //Height of this section, to choose its column.
                column = 0;
                for (int c = 1; c < columns; c++) { if (columnY[c] < columnY[column] - 1) { column = c; } }
                Head h;
                h.title = builtFrench ? KEY_ROWS[i].fr : KEY_ROWS[i].en;
                h.x = body.UpperLeftCorner.X + column * (columnWidth + gap);
                h.y = columnY[column];
                h.section = section;
                heads.push_back(h);
                columnY[column] += 44;
                continue;
            }
            Row r;
            r.caps = parseKeys(KEY_ROWS[i].keys, builtFrench);
            r.x = body.UpperLeftCorner.X + column * (columnWidth + gap);
            r.y = columnY[column];
            r.desc = wrapText(textFont, builtFrench ? KEY_ROWS[i].fr : KEY_ROWS[i].en, (irr::s32)(columnWidth - keyColumn - 18));
            //Caps may wrap onto a second line when a combination is wider than the key column.
            irr::f32 capLines = 1, cx = 0;
            for (size_t c = 0; c < r.caps.size(); c++) {
                if (cx + r.caps[c].w > keyColumn && cx > 0) { capLines++; cx = 0; }
                cx += r.caps[c].w + 6;
            }
            r.h = irr::core::max_(capLines * (capH + 6) - 6, r.desc.size() * lineH) + 12;
            columnY[column] += r.h;
            rows.push_back(r);
            if (i + 1 < KEY_ROW_COUNT && KEY_ROWS[i + 1].keys == 0) { columnY[column] += 14; } //space after a section
        }
        irr::f32 contentH = 0;
        for (int c = 0; c < columns; c++) { contentH = irr::core::max_(contentH, columnY[c]); }
        maxScroll = irr::core::max_(0.0f, contentH - body.getHeight());
        scroll = irr::core::clamp(scroll, 0.0f, maxScroll);
    }

    irr::gui::IGUIFont* titleFont;
    irr::gui::IGUIFont* textFont;
    irr::gui::IGUIFont* smallFont;
    std::vector<Row> rows;
    std::vector<Head> heads;
    irr::core::rect<irr::f32> panel, body, closeButton, langSwitch, langFr, langEn, doneButton;
    irr::core::vector2df badgeCentre;
    irr::f32 columnWidth, keyColumn;
    irr::f32 scroll, maxScroll;
    bool builtFrench;
    irr::s32 builtWidth, builtHeight;
};

KeySheet* g_keySheet = 0;
ScreenPicker* g_screenPicker = 0;

bool overlayOpen()
{
    return (g_keySheet && g_keySheet->isVisible()) || (g_screenPicker && g_screenPicker->isVisible());
}

//-------------------------------------------------------------------------------------------------
//Starting the simulator, and the bc5.ini settings this needs.
//-------------------------------------------------------------------------------------------------

//The current value of a key in an ini file. Read from the file each time: IniFile caches what it has
//read, and the settings editor may have changed the file since the launcher started.
std::string readIniNow(const std::string& path, const std::string& key)
{
    std::ifstream in(path.c_str());
    std::string line, wanted = key;
    Utilities::to_lower(wanted);
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) { continue; }
        std::string name = Utilities::trim(line.substr(0, eq));
        Utilities::to_lower(name);
        if (name == wanted) { return Utilities::trim(Utilities::trim(line.substr(eq + 1)), "\""); }
    }
    return "";
}

//The bc5.ini in use: the user's copy if there is one, else the installed file.
std::string currentBc5Ini()
{
    return Utilities::pathExists(userFolder + "bc5.ini") ? userFolder + "bc5.ini" : std::string("bc5.ini");
}

//Where the screen picker's last choice is kept (offered again next time).
std::string launcherScreensFile()
{
    return Utilities::getUserDir() + "launcherScreens.ini";
}

//Keeps a screen picker choice for next time: Bridge, Instruments or Repeater (the others are kept).
bool saveLauncherScreen(const std::string& key, int value)
{
    const char* keys[3] = { "Bridge", "Instruments", "Repeater" };
    std::string values[3];
    for (int k = 0; k < 3; k++) { values[k] = (key == keys[k]) ? std::to_string(value) : readIniNow(launcherScreensFile(), keys[k]); }

    //User folder, as the settings editor creates it.
    const std::string dirs[2] = { Utilities::getUserDirBase(), Utilities::getUserDir() };
    for (int d = 0; d < 2; d++) {
        std::string dir = dirs[d];
        if (dir.size() > 1 && !Utilities::pathExists(dir)) {
            dir.erase(dir.size() - 1); //no trailing slash
#ifdef _WIN32
            _mkdir(dir.c_str());
#else
            mkdir(dir.c_str(), 0755);
#endif
        }
    }
    std::ofstream out(launcherScreensFile().c_str(), std::ios::trunc);
    for (int k = 0; k < 3; k++) {
        if (!values[k].empty()) { out << keys[k] << "=" << values[k] << "\n"; }
    }
    return out.good();
}

//Starts the simulator. When screens were picked: the bridge view's (1..n) as "-monitor N", and the
//instrument console's as "-console M" (0: in the bridge view).
void launchSimulator(int screen, int consoleScreen)
{
    const std::string screenArg = std::to_string(screen);
    const std::string consoleArg = std::to_string(consoleScreen);
#ifdef _WIN32
    const std::string params = "-monitor " + screenArg + " -console " + consoleArg;
    ShellExecute(NULL, NULL, "Simulator-nav.exe", screen > 0 ? params.c_str() : NULL, NULL, SW_SHOW);
#else
    const int pid = fork(); // posix only (GNU/Linux, MacOS)
    if (pid != 0) { return; }
#ifdef FOR_DEB
    if (chdir("/usr/bin") != 0) {} //the applications are installed there
#endif
#ifdef __APPLE__
    if (screen > 0) { execl("../MacOS/bc.app/Contents/MacOS/bc", "bc", "-monitor", screenArg.c_str(), "-console", consoleArg.c_str(), (char*)NULL); }
    else { execl("../MacOS/bc.app/Contents/MacOS/bc", "bc", (char*)NULL); }
#else
    if (screen > 0) { execl("./Simulator-bc", "Simulator-bc", "-monitor", screenArg.c_str(), "-console", consoleArg.c_str(), (char*)NULL); }
    else { execl("./Simulator-bc", "Simulator-bc", (char*)NULL); }
#endif
    _exit(EXIT_FAILURE); //only reached if the simulator could not be started: never run a second launcher
#endif
}

//Simulator card: on a desk with several screens (borderless full screen), always ask which ones, the
//last choice offered again (at first, the screens set in bc5.ini); otherwise start straight away.
void startSimulator()
{
    const std::string ini = currentBc5Ini();
    const bool borderless = readIniNow(ini, "graphics_mode") == "3";
    const std::vector<ScreenInfo> screens = listScreens();
    if (g_screenPicker && borderless && screens.size() > 1) {
        int bridge = atoi(readIniNow(launcherScreensFile(), "Bridge").c_str());
        int console = atoi(readIniNow(launcherScreensFile(), "Instruments").c_str());
        if (bridge <= 0) {
            bridge = atoi(readIniNow(ini, "monitor").c_str());
            console = atoi(readIniNow(ini, "console_monitor").c_str());
        }
        g_screenPicker->open(screens, bridge, console);
        return;
    }
    showLaunchToast(g_simulatorTitle);
    launchSimulator(0, 0);
}

std::wstring g_repeaterTitle; //the repeater card's title, for the toast once a screen has been picked

//Starts the gyro repeater; screen (1..n) is passed on as "-monitor N" when one was picked.
void launchRepeater(int screen)
{
    const std::string screenArg = std::to_string(screen);
#ifdef _WIN32
    const std::string params = "-monitor " + screenArg;
    ShellExecute(NULL, NULL, "Simulator-rp.exe", screen > 0 ? params.c_str() : NULL, NULL, SW_SHOW);
#else
    const int pid = fork(); // posix only (GNU/Linux, MacOS)
    if (pid != 0) { return; }
#ifdef FOR_DEB
    if (chdir("/usr/bin") != 0) {} //the applications are installed there
#endif
#ifdef __APPLE__
    if (screen > 0) { execl("../MacOS/rp.app/Contents/MacOS/rp", "rp", "-monitor", screenArg.c_str(), (char*)NULL); }
    else { execl("../MacOS/rp.app/Contents/MacOS/rp", "rp", (char*)NULL); }
#else
    if (screen > 0) { execl("./Simulator-rp", "Simulator-rp", "-monitor", screenArg.c_str(), (char*)NULL); }
    else { execl("./Simulator-rp", "Simulator-rp", (char*)NULL); }
#endif
    _exit(EXIT_FAILURE); //only reached if the repeater could not be started: never run a second launcher
#endif
}

//Repeater card: on a desk with several screens (borderless full screen in repeater.ini), ask which one,
//the last choice offered again (at first, the screen set in repeater.ini); otherwise start straight away.
void startRepeater()
{
    const std::string ini = Utilities::pathExists(userFolder + "repeater.ini") ? userFolder + "repeater.ini" : std::string("repeater.ini");
    const bool borderless = readIniNow(ini, "graphics_mode") == "3";
    const std::vector<ScreenInfo> screens = listScreens();
    if (g_screenPicker && borderless && screens.size() > 1) {
        int last = atoi(readIniNow(launcherScreensFile(), "Repeater").c_str());
        if (last <= 0) { last = atoi(readIniNow(ini, "monitor").c_str()); }
        g_screenPicker->openForRepeater(screens, last);
        return;
    }
    showLaunchToast(g_repeaterTitle);
    launchRepeater(0);
}

//-------------------------------------------------------------------------------------------------
//Starting the other applications
//-------------------------------------------------------------------------------------------------

//Starts one of the applications (arg may be 0).
void startApplication(const char* windowsExe, const char* macApp, const char* posixExe, const char* arg)
{
#ifdef _WIN32
    ShellExecute(NULL, NULL, windowsExe, arg, NULL, SW_SHOW);
#else
    const int pid = fork(); // posix only (GNU/Linux, MacOS)
    if (pid != 0) { return; }
#ifdef __APPLE__
    const std::string path = std::string("../MacOS/") + macApp + ".app/Contents/MacOS/" + macApp;
    if (arg) { execl(path.c_str(), macApp, arg, (char*)NULL); }
    else { execl(path.c_str(), macApp, (char*)NULL); }
#else
#ifdef FOR_DEB
    if (chdir("/usr/bin") != 0) {} //the applications are installed there
#endif
    const std::string path = std::string("./") + posixExe;
    if (arg) { execl(path.c_str(), posixExe, arg, (char*)NULL); }
    else { execl(path.c_str(), posixExe, (char*)NULL); }
#endif
    _exit(EXIT_FAILURE); //only reached if the application could not be started: never run a second launcher
#endif
}

void openDocumentation()
{
#ifdef _WIN32
    ShellExecute(NULL, TEXT("open"), TEXT("doc\\index.html"), NULL, NULL, SW_SHOWNORMAL);
#else
    const int pid = fork(); // posix only (GNU/Linux, MacOS)
    if (pid != 0) { return; }
#ifdef __APPLE__
    execl("/usr/bin/open", "open", "../Resources/doc/index.html", (char*)NULL);
#else
#ifdef FOR_DEB
    execl("/usr/bin/xdg-open", "xdg-open", "/usr/share/doc/Simulator/index.html", (char*)NULL);
#else
    execl("/usr/bin/xdg-open", "xdg-open", "doc/index.html", (char*)NULL);
#endif
    //Failed to open the help: the online documentation instead
    if (chdir("/usr/bin") != 0) {} // If firefox is running in a snap or similar, launching can fail if it can't access the current dir
    execl("/usr/bin/xdg-open", "xdg-open", "https://www.bridgecommand.co.uk/Documentation", (char*)NULL);
#endif
    _exit(EXIT_FAILURE);
#endif
}

//What a menu item does. title: for the "launching" message.
void runAction(irr::s32 id, const std::wstring& title)
{
    switch (id) {
    case BC_BUTTON:
        //may first ask which screens to use (see startSimulator)
        g_simulatorTitle = title;
        startSimulator();
        return;
    case RP_BUTTON:
        //may first ask which screen to use (see startRepeater)
        g_repeaterTitle = title;
        startRepeater();
        return;
    case KEYS_BUTTON:
        if (g_keySheet) { g_keySheet->open(); }
        return;
    case DOC_BUTTON:
        openDocumentation();
        return;
    default:
        break;
    }
    showLaunchToast(title);
    switch (id) {
    case MC_BUTTON: startApplication("Simulator-mc.exe", "mc", "Simulator-mc", 0); break;
    case ED_BUTTON: startApplication("Simulator-ed.exe", "ed", "Simulator-ed", 0); break;
    case FE_BUTTON: startApplication("Simulator-fe.exe", "fe", "Simulator-fe", 0); break;
    case MH_BUTTON: startApplication("Simulator-mh.exe", "mh", "Simulator-mh", 0); break;
    case INI_BC_BUTTON: startApplication("Simulator-ini.exe", "ini", "Simulator-ini", 0); break;
    case INI_MC_BUTTON: startApplication("Simulator-ini.exe", "ini", "Simulator-ini", "-M"); break;
    case INI_MH_BUTTON: startApplication("Simulator-ini.exe", "ini", "Simulator-ini", "-H"); break;
    default: break;
    }
}

//-------------------------------------------------------------------------------------------------
//The launcher window: full screen (the whole screen, no border) or an ordinary window
//-------------------------------------------------------------------------------------------------

#ifdef _WIN32
//Real pixels: sharp lettering on screens Windows scales to 125 %, 150 % or more (otherwise it draws the
//launcher smaller and stretches it). Looked up at run time, for older Windows.
void becomeDpiAware()
{
    HMODULE user = GetModuleHandleA("user32.dll");
    if (!user) { return; }
    typedef BOOL(WINAPI* ContextFn)(HANDLE);
    ContextFn setContext = (ContextFn)(void*)GetProcAddress(user, "SetProcessDpiAwarenessContext");
    if (setContext && setContext((HANDLE)(INT_PTR)-4)) { return; } //per monitor, version 2
    typedef BOOL(WINAPI* AwareFn)();
    AwareFn setAware = (AwareFn)(void*)GetProcAddress(user, "SetProcessDPIAware");
    if (setAware) { setAware(); }
}
#endif

//The screen the launcher opens on: the one under the mouse.
irr::core::rect<irr::s32> launcherScreenArea()
{
#ifdef _WIN32
    POINT cursor;
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (GetCursorPos(&cursor) && GetMonitorInfo(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &mi)) {
        return irr::core::rect<irr::s32>(mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right, mi.rcMonitor.bottom);
    }
#endif
    irr::core::dimension2du desk(0, 0);
    if (g_device) {
        desk = g_device->getVideoModeList()->getDesktopResolution();
#if !defined(_WIN32) && !defined(__APPLE__)
        struct _XDisplay* display = (struct _XDisplay*)g_device->getVideoDriver()->getExposedVideoData().OpenGLLinux.X11Display;
        if ((desk.Width == 0 || desk.Height == 0) && display) {
            const int screen = XDefaultScreen(display);
            desk = irr::core::dimension2du((irr::u32)XDisplayWidth(display, screen), (irr::u32)XDisplayHeight(display, screen));
        }
#endif
    }
    else {
        irr::IrrlichtDevice* nulldevice = irr::createDevice(irr::video::EDT_NULL);
        if (nulldevice) {
            desk = nulldevice->getVideoModeList()->getDesktopResolution();
            nulldevice->drop();
        }
    }
    if (desk.Width == 0 || desk.Height == 0) { desk = irr::core::dimension2du(1280, 720); } //known once the window is open
    return irr::core::rect<irr::s32>(0, 0, (irr::s32)desk.Width, (irr::s32)desk.Height);
}

//Window size for the windowed mode: 1280 x 720, or 90 % of a smaller screen.
irr::core::dimension2du windowedSize(const irr::core::rect<irr::s32>& area)
{
    return irr::core::dimension2du((irr::u32)irr::core::min_(1280, (irr::s32)(area.getWidth() * 0.9f)),
        (irr::u32)irr::core::min_(720, (irr::s32)(area.getHeight() * 0.9f)));
}

void setWindowMode(bool fullScreen)
{
    if (!g_device) { return; }
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(g_device->getVideoDriver()->getExposedVideoData().OpenGLWin32.HWnd);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (!hwnd || !GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) { return; }
    if (fullScreen) {
        SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
        SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    else {
        const irr::core::rect<irr::s32> work(mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom);
        const irr::core::dimension2du client = windowedSize(work);
        RECT frame = { 0, 0, (LONG)client.Width, (LONG)client.Height };
        const DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        AdjustWindowRect(&frame, style, FALSE);
        const int w = frame.right - frame.left, h = frame.bottom - frame.top;
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
        SetWindowPos(hwnd, HWND_NOTOPMOST, work.UpperLeftCorner.X + (work.getWidth() - w) / 2, work.UpperLeftCorner.Y + (work.getHeight() - h) / 2,
            w, h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
#else
    const irr::core::rect<irr::s32> area = launcherScreenArea();
    g_device->setWindowSize(fullScreen ? irr::core::dimension2du((irr::u32)area.getWidth(), (irr::u32)area.getHeight()) : windowedSize(area));
    g_device->setResizable(!fullScreen);
#endif
}

//This computer's name, shown at the top of the menu.
std::wstring stationName()
{
#ifdef _WIN32
    wchar_t name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(name, &size)) { return std::wstring(name, size); }
    return L"";
#else
    char name[256];
    if (gethostname(name, sizeof(name)) != 0) { return L""; }
    name[sizeof(name) - 1] = 0;
    return std::wstring(name, name + strlen(name));
#endif
}

//Text read from a file: UTF-8 if it is valid UTF-8, else Latin-1.
std::wstring decodeText(const std::string& s)
{
    std::wstring out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = (unsigned char)s[i];
        int extra = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : (c >= 0x80) ? -1 : 0;
        if (extra < 0 || i + extra >= s.size() + (extra == 0 ? 1 : 0)) { extra = -1; }
        unsigned long cp = (extra == 1) ? (c & 0x1F) : (extra == 2) ? (c & 0x0F) : (extra == 3) ? (c & 0x07) : c;
        for (int k = 1; k <= extra && extra > 0; k++) {
            const unsigned char d = (unsigned char)s[i + k];
            if ((d & 0xC0) != 0x80) { extra = -1; break; }
            cp = (cp << 6) | (d & 0x3F);
        }
        if (extra < 0) {
            //Not UTF-8: the whole text as Latin-1.
            out.clear();
            for (size_t j = 0; j < s.size(); j++) { out += (wchar_t)(unsigned char)s[j]; }
            return out;
        }
        out += (wchar_t)cp;
        i += extra + 1;
    }
    return out;
}

//Facts shown with the simulator item: the screens, and the last exercise run on this computer.
std::vector<std::pair<std::wstring, std::wstring> > simulatorFacts(bool french)
{
    std::vector<std::pair<std::wstring, std::wstring> > facts;
    const size_t screens = irr::core::max_((size_t)1, listScreens().size());
    facts.push_back(std::make_pair(std::wstring(french ? L"\u00C9CRANS" : L"SCREENS"),
        std::to_wstring(screens) + (french ? (screens > 1 ? L" \u00E9crans, choix au lancement" : L" \u00E9cran") : (screens > 1 ? L" screens, chosen at start" : L" screen"))));
    const std::string last = readIniNow(Utilities::getUserDir() + "lastScenario.ini", "Scenario");
    if (!last.empty()) { facts.push_back(std::make_pair(std::wstring(french ? L"DERNIER EXERCICE" : L"LAST EXERCISE"), decodeText(last))); }
    return facts;
}

//A line in launcher.log (user folder): why a film or the music could not be played.
void logLine(const std::string& line)
{
    std::ofstream out((Utilities::getUserDir() + "launcher.log").c_str(), std::ios::app);
    out << line << "\n";
}

//While the intro film plays: a key or a click skips it.
class Receiver : public irr::IEventReceiver
{
public:
    Receiver() : intro(false), skip(false) {}

    virtual bool OnEvent(const irr::SEvent& event)
    {
        if (!intro) { return false; }
        if (event.EventType == irr::EET_KEY_INPUT_EVENT) {
            if (event.KeyInput.PressedDown) { skip = true; }
            return true;
        }
        if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
            if (event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN || event.MouseInput.Event == irr::EMIE_RMOUSE_PRESSED_DOWN) { skip = true; }
            return true;
        }
        return false;
    }

    bool intro, skip;
};

int main(int argc, char** argv)
{

    if ((argc > 1) && (strcmp(argv[1], "--version") == 0)) {
        std::cout << LONGVERSION << std::endl;
        exit(EXIT_SUCCESS);
    }

#ifdef FOR_DEB
    if (chdir("/usr/share/Simulator") != 0) {}
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
    const bool french = (modifier == "fr");
    g_french = french;

    float fontScale = IniFile::iniFileTof32(iniFilename, "font_scale");
    if (fontScale < 1) {
        fontScale = 1.0;
    }

    //Launcher options (intro film, sounds, music, full screen); --no-intro and --windowed for one run.
    LauncherOptions options;
    options.load();
    bool noIntro = false, windowedRun = false;
    for (int arg = 1; arg < argc; arg++) {
        if (strcmp(argv[arg], "--no-intro") == 0) { noIntro = true; }
        if (strcmp(argv[arg], "--windowed") == 0) { windowedRun = true; }
    }
    const bool fullScreen = options.fullScreen && !windowedRun;

    //The window: the whole screen under the mouse, or a window in its middle.
#ifdef _WIN32
    becomeDpiAware();
#endif
    const irr::core::rect<irr::s32> area = launcherScreenArea();
    irr::SIrrlichtCreationParameters parameters;
    parameters.DriverType = irr::video::EDT_OPENGL;
    parameters.Bits = 32;
    parameters.Fullscreen = false;
    parameters.Stencilbuffer = false;
    parameters.Vsync = true;
    if (fullScreen) {
        parameters.WindowSize = irr::core::dimension2du((irr::u32)area.getWidth(), (irr::u32)area.getHeight());
        parameters.WindowPosition = area.UpperLeftCorner;
    }
    else {
        parameters.WindowSize = windowedSize(area);
        parameters.WindowPosition = irr::core::position2di(area.UpperLeftCorner.X + (area.getWidth() - (irr::s32)parameters.WindowSize.Width) / 2,
            area.UpperLeftCorner.Y + (area.getHeight() - (irr::s32)parameters.WindowSize.Height) / 2);
    }
    irr::IrrlichtDevice* device = irr::createDeviceEx(parameters);
    if (!device) {
        parameters.Vsync = false;
        device = irr::createDeviceEx(parameters);
    }
    if (!device) {
        return EXIT_FAILURE;
    }
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::gui::IGUIEnvironment* env = device->getGUIEnvironment();
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
    device->setWindowCaption(L"NAUTITECH - Simulateur de Navigation Maritime");

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

    if (fullScreen) { setWindowMode(true); }
    else { device->setResizable(true); }

    //Black straight away, while the rest loads.
    driver->beginScene(irr::video::ECBF_COLOR | irr::video::ECBF_DEPTH, irr::video::SColor(255, 0, 0, 0));
    driver->endScene();

    //Bitmap fonts: the simulator's font (bc5.ini font=, normally noto-sans), for the screen picker and
    //the key sheet, and if the menu's TrueType faces are missing.
    std::string fontName = IniFile::iniFileToString(iniFilename, "font");
    if (fontName.empty()) { fontName = "noto-sans"; }
    //(larger on large screens: the screen picker and the key sheet are drawn in pixels)
    const float screenScale = irr::core::clamp(area.getHeight() / 1080.0f, 1.0f, 2.0f);
    auto loadFont = [&](int size) -> irr::gui::IGUIFont* {
        size = (int)(size * fontScale * screenScale + 0.5f);
        if (size > 36) { size = 36; }
        const std::string path = "media/fonts/" + fontName + "/" + fontName + "-" + std::to_string(size) + ".xml";
        return env->getFont(path.c_str());
    };
    irr::gui::IGUIFont* titleFont = loadFont(20);
    irr::gui::IGUIFont* textFont = loadFont(15);
    irr::gui::IGUIFont* smallFont = loadFont(13);
    irr::gui::IGUIFont* bigFont = loadFont(30);

    //Dark skin, for the keyboard-shortcut sheet.
    irr::gui::IGUISkin* skin = env->createSkin(irr::gui::EGST_WINDOWS_CLASSIC);
    env->setSkin(skin);
    skin->drop();
    skin->setColor(irr::gui::EGDC_WINDOW, irr::video::SColor(255, 20, 28, 40));
    skin->setColor(irr::gui::EGDC_3D_FACE, irr::video::SColor(255, 34, 46, 62));
    skin->setColor(irr::gui::EGDC_3D_SHADOW, irr::video::SColor(255, 12, 18, 26));
    skin->setColor(irr::gui::EGDC_3D_DARK_SHADOW, irr::video::SColor(255, 8, 12, 18));
    skin->setColor(irr::gui::EGDC_3D_HIGH_LIGHT, irr::video::SColor(255, 60, 76, 96));
    skin->setColor(irr::gui::EGDC_3D_LIGHT, irr::video::SColor(255, 48, 62, 80));
    skin->setColor(irr::gui::EGDC_ACTIVE_BORDER, irr::video::SColor(255, 40, 120, 210));
    skin->setColor(irr::gui::EGDC_ACTIVE_CAPTION, irr::video::SColor(255, 244, 247, 251));
    skin->setColor(irr::gui::EGDC_INACTIVE_BORDER, irr::video::SColor(255, 34, 46, 62));
    skin->setColor(irr::gui::EGDC_INACTIVE_CAPTION, irr::video::SColor(255, 178, 192, 208));
    skin->setColor(irr::gui::EGDC_BUTTON_TEXT, irr::video::SColor(255, 244, 247, 251));
    skin->setColor(irr::gui::EGDC_HIGH_LIGHT, irr::video::SColor(255, 40, 120, 210));
    skin->setColor(irr::gui::EGDC_HIGH_LIGHT_TEXT, irr::video::SColor(255, 255, 255, 255));
    skin->setColor(irr::gui::EGDC_EDITABLE, irr::video::SColor(255, 14, 20, 30));
    skin->setColor(irr::gui::EGDC_FOCUSED_EDITABLE, irr::video::SColor(255, 18, 26, 38));
    skin->setColor(irr::gui::EGDC_GRAY_TEXT, irr::video::SColor(255, 130, 144, 160));
    skin->setColor(irr::gui::EGDC_WINDOW_SYMBOL, irr::video::SColor(255, 220, 228, 238));
    skin->setColor(irr::gui::EGDC_TOOLTIP, irr::video::SColor(255, 236, 242, 248));
    skin->setColor(irr::gui::EGDC_TOOLTIP_BACKGROUND, irr::video::SColor(235, 18, 30, 46));
    if (textFont) { skin->setFont(textFont); }

    //Background picture: media/menu_background.jpg (or .png) made for this menu, else bc5.ini
    //launcher_image=<file> (in media/ or a path), else the usual one. (A film, menu_background.mp4,
    //is played over it once the menu is open.)
    std::string bgName = Utilities::pathExists("media/menu_background.jpg") ? std::string("media/menu_background.jpg")
        : (Utilities::pathExists("media/menu_background.png") ? std::string("media/menu_background.png") : std::string(""));
    if (bgName.empty()) { bgName = IniFile::iniFileToString(iniFilename, "launcher_image"); }
    if (bgName.empty()) { bgName = "media/bg_main.png"; }
    else if (!Utilities::pathExists(bgName) && Utilities::pathExists("media/" + bgName)) { bgName = "media/" + bgName; }
    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, true);
    irr::video::ITexture* bgTex = driver->getTexture(bgName.c_str());

    // Trim trailing/leading whitespace from language strings so labels sit where they should
    auto trimLabel = [](std::wstring s) -> std::wstring {
        size_t start = s.find_first_not_of(L" \t\r\n");
        size_t end = s.find_last_not_of(L" \t\r\n");
        return (start == std::wstring::npos) ? L"" : s.substr(start, end - start + 1);
        };
    auto T = [&](const std::string& key) -> std::wstring {
        std::wstring ws = language.translate(key.c_str()).c_str();
        return trimLabel(ws);
        };
    //Optional phrase: from the language file if it has it, else the built-in French / English.
    auto tr = [&](const std::string& key, const wchar_t* fr, const wchar_t* en) -> std::wstring {
        const std::wstring ws = trimLabel(IniFile::iniFileToWString(languageFile, key));
        return ws.empty() ? std::wstring(french ? fr : en) : ws;
        };

    //Sounds of the menu
    UiSounds sounds;
    sounds.load();
    sounds.setEnabled(options.uiSounds);

    //Films: the intro, then (optional) a looping menu background and menu music.
    VideoClip intro, film, music;
    auto startMusic = [&]() {
        const std::string file = VideoClip::findAudio("media/menu_music");
        if (!file.empty() && music.open(file, false, true, true)) { music.setVolume(0.6f); }
        else if (!file.empty()) { logLine("Menu music " + file + ": cannot be played here"); }
    };

    //The menu
    HudMenu* hud = 0;
    hud = new HudMenu(env, french, "media/fonts/barlow-condensed/", textFont, &sounds, &options,
        [&](const HudItem& item) {
            switch (item.id) {
            case HUD_QUIT:
                device->closeDevice();
                return;
            case HUD_MINIMISE:
                device->minimizeWindow();
                return;
            case HUD_TOGGLE_FULLSCREEN:
                options.fullScreen = !options.fullScreen;
                options.save();
                setWindowMode(options.fullScreen);
                hud->setWindowControls(options.fullScreen);
                return;
            default:
                break;
            }
            if (item.option == HudMenu::Option_FullScreen) {
                setWindowMode(options.fullScreen);
                hud->setWindowControls(options.fullScreen);
            }
            else if (item.option == HudMenu::Option_Music) {
                if (options.menuMusic) { startMusic(); } else { music.close(); }
            }
            else if (item.option < 0) {
                runAction(item.id, item.title.empty() ? item.label : item.title);
            }
        },
        []() { return overlayOpen(); });
    hud->drop(); //the GUI tree holds it
    g_hud = hud;
    hud->setWindowControls(fullScreen);
    std::wstring version = L"NAUTITECH";
    if (!LONGVERSION.empty()) { version += L"  \u00B7  v" + std::wstring(irr::core::stringw(LONGVERSION.c_str()).c_str()); }
    hud->setStatus(stationName(), version);
    hud->setBackground(bgTex, 0);

    const int mainPage = hud->addPage(french ? L"Menu principal" : L"Main menu");
    const int settingsPage = hud->addPage(french ? L"Param\u00E8tres" : L"Settings");

    const std::wstring launch = french ? L"LANCER" : L"LAUNCH";
    const std::wstring openText = french ? L"OUVRIR" : L"OPEN";
    {
        HudItem item;
        item.id = BC_BUTTON;
        item.label = tr("hudBC", L"Simulateur", L"Simulator");
        item.title = T("startBC");
        item.text = tr("startBCInfo", L"Lancer un exercice : passerelle, radar et instruments", L"Run an exercise: bridge view, radar and instruments");
        item.tags = french ? std::vector<std::wstring>{ L"PASSERELLE 3D", L"RADAR", L"INSTRUMENTS", L"MULTI-\u00C9CRANS" }
            : std::vector<std::wstring>{ L"3D BRIDGE", L"RADAR", L"INSTRUMENTS", L"MULTI-SCREEN" };
        item.facts = simulatorFacts(french);
        item.action = launch;
        item.icon = Icon_Helm;
        item.launches = true;
        hud->addItem(mainPage, item);

        item = HudItem();
        item.id = RP_BUTTON;
        item.label = tr("hudRP", L"R\u00E9p\u00E9titeur gyro", L"Gyro repeater");
        item.title = T("startRP");
        item.text = tr("startRPInfo", L"Cap du navire sur un \u00E9cran \u00E0 part (ou angle de barre)", L"Ship's heading on a screen of its own (or rudder angle)");
        item.tags = french ? std::vector<std::wstring>{ L"CAP", L"ANGLE DE BARRE", L"\u00C9CRAN D\u00C9DI\u00C9" }
            : std::vector<std::wstring>{ L"HEADING", L"RUDDER ANGLE", L"OWN SCREEN" };
        item.action = launch;
        item.icon = Icon_Compass;
        item.launches = true;
        hud->addItem(mainPage, item);

        item = HudItem();
        item.id = ED_BUTTON;
        item.label = tr("hudED", L"\u00C9diteur de sc\u00E9nario", L"Scenario editor");
        item.title = T("startED");
        item.text = tr("startEDInfo", L"Cr\u00E9er et modifier les exercices de navigation", L"Create and edit navigation exercises");
        item.tags = french ? std::vector<std::wstring>{ L"NAVIRES", L"ROUTES", L"M\u00C9T\u00C9O", L"CARTE" }
            : std::vector<std::wstring>{ L"SHIPS", L"ROUTES", L"WEATHER", L"CHART" };
        item.action = openText;
        item.icon = Icon_Route;
        item.launches = true;
        hud->addItem(mainPage, item);

        item = HudItem();
        item.id = FE_BUTTON;
        item.label = tr("hudFE", L"Incendie / SAR", L"Fire / SAR");
        item.title = T("startFE");
        item.text = tr("startFEInfo", L"Incendie \u00E0 bord, naufrag\u00E9s et moyens SAR", L"Fire on board, survivors and SAR units");
        item.tags = french ? std::vector<std::wstring>{ L"INCENDIE", L"NAUFRAG\u00C9S", L"MOYENS SAR" }
            : std::vector<std::wstring>{ L"FIRE", L"SURVIVORS", L"SAR UNITS" };
        item.action = openText;
        item.icon = Icon_Flame;
        item.launches = true;
        hud->addItem(mainPage, item);

        item = HudItem();
        item.id = MH_BUTTON;
        item.label = tr("hudMH", L"Multijoueur", L"Multiplayer");
        item.title = T("startMH");
        item.text = tr("startMHInfo", L"Relier plusieurs postes pour un exercice commun", L"Link several stations in one exercise");
        item.tags = french ? std::vector<std::wstring>{ L"R\u00C9SEAU", L"PLUSIEURS POSTES" } : std::vector<std::wstring>{ L"NETWORK", L"SEVERAL STATIONS" };
        item.action = openText;
        item.icon = Icon_Network;
        item.launches = true;
        hud->addItem(mainPage, item);

        item = HudItem();
        item.id = KEYS_BUTTON;
        item.label = french ? L"Raccourcis clavier" : L"Keyboard shortcuts";
        item.title = item.label;
        item.text = french ? L"Toutes les touches du simulateur, en fran\u00E7ais et en anglais : \u00E0 lire avant une s\u00E9ance, ou \u00E0 montrer aux stagiaires."
            : L"All the simulator's keys, in French and in English: to read before a session, or to show the trainees.";
        item.tags = french ? std::vector<std::wstring>{ L"FR / EN" } : std::vector<std::wstring>{ L"FR / EN" };
        item.action = french ? L"AFFICHER" : L"SHOW";
        item.icon = Icon_Keys;
        hud->addItem(mainPage, item);

        item = HudItem();
        item.label = french ? L"Param\u00E8tres" : L"Settings";
        item.title = item.label;
        item.text = french ? L"R\u00E9glages du simulateur, de la carte et du hub (mot de passe administrateur), et options du lanceur : vid\u00E9o d'introduction, sons, musique, plein \u00E9cran."
            : L"Simulator, chart and hub settings (administrator password), and launcher options: intro film, sounds, music, full screen.";
        item.action = openText;
        item.icon = Icon_Gear;
        item.page = settingsPage;
        hud->addItem(mainPage, item);

        item = HudItem();
        item.label = T("leave");
        if (item.label.empty() || item.label == L"leave") { item.label = french ? L"Quitter" : L"Quit"; }
        item.title = item.label;
        item.text = french ? L"Fermer le lanceur. Les applications d\u00E9j\u00E0 ouvertes continuent." : L"Close the launcher. Applications already open keep running.";
        item.action = french ? L"QUITTER" : L"QUIT";
        item.icon = Icon_Power;
        item.quit = true;
        hud->addItem(mainPage, item);

        //Settings
        const std::wstring adminText = french ? L"R\u00E9serv\u00E9 aux administrateurs : le mot de passe est demand\u00E9 \u00E0 l'ouverture."
            : L"Administrators only: the password is asked for when it opens.";
        item = HudItem();
        item.id = INI_BC_BUTTON;
        item.label = tr("hudINIBC", L"R\u00E9glages du simulateur", L"Simulator settings");
        item.title = item.label;
        item.text = (french ? L"Affichage, \u00E9crans, langue, son et fonctionnement du simulateur. " : L"Display, screens, language, sound and how the simulator runs. ") + adminText;
        item.action = openText;
        item.icon = Icon_Gear;
        item.locked = true;
        item.launches = true;
        hud->addItem(settingsPage, item);

        item.id = INI_MC_BUTTON;
        item.label = tr("hudINIMC", L"R\u00E9glages de la carte", L"Chart settings");
        item.title = item.label;
        item.text = (french ? L"R\u00E9glages de la carte du contr\u00F4leur. " : L"Settings of the controller's chart. ") + adminText;
        hud->addItem(settingsPage, item);

        item.id = INI_MH_BUTTON;
        item.label = tr("hudINIMH", L"R\u00E9glages du hub", L"Hub settings");
        item.title = item.label;
        item.text = (french ? L"R\u00E9glages du hub multijoueurs. " : L"Settings of the multiplayer hub. ") + adminText;
        hud->addItem(settingsPage, item);

        const std::wstring change = french ? L"CHANGER" : L"SWITCH";
        auto fileFact = [&](const std::string& found, const wchar_t* expected) {
            return std::make_pair(std::wstring(french ? L"FICHIER" : L"FILE"),
                found.empty() ? std::wstring(expected) + (french ? L" (absent)" : L" (missing)") : std::wstring(found.begin(), found.end()));
        };
        item = HudItem();
        item.label = french ? L"Vid\u00E9o d'introduction" : L"Intro film";
        item.title = item.label;
        item.text = french ? L"Film jou\u00E9 en plein \u00E9cran au d\u00E9marrage du lanceur ; une touche ou un clic le passe. Sans fichier, le menu s'ouvre directement."
            : L"Film played full screen when the launcher starts; a key or a click skips it. Without the file, the menu opens straight away.";
        item.facts.push_back(fileFact(VideoClip::find("media/intro"), L"media/intro.mp4"));
        item.action = change;
        item.icon = Icon_Compass;
        item.option = HudMenu::Option_Intro;
        hud->addItem(settingsPage, item);

        item = HudItem();
        item.label = french ? L"Sons de l'interface" : L"Interface sounds";
        item.title = item.label;
        item.text = french ? L"Sons au survol et au clic dans le menu. Rempla\u00E7ables par vos fichiers : media/sounds/ui_hover.wav, ui_select.wav, ui_back.wav, ui_open.wav, ui_launch.wav."
            : L"Sounds when moving over and choosing menu items. Your own files can replace them: media/sounds/ui_hover.wav, ui_select.wav, ui_back.wav, ui_open.wav, ui_launch.wav.";
        item.action = change;
        item.icon = Icon_Network;
        item.option = HudMenu::Option_Sounds;
        hud->addItem(settingsPage, item);

        item = HudItem();
        item.label = french ? L"Musique du menu" : L"Menu music";
        item.title = item.label;
        item.text = french ? L"Musique en boucle pendant le menu, arr\u00EAt\u00E9e pendant les exercices." : L"Music looping while the menu is shown, stopped during exercises.";
        item.facts.push_back(fileFact(VideoClip::findAudio("media/menu_music"), L"media/menu_music.mp3"));
        item.action = change;
        item.icon = Icon_Route;
        item.option = HudMenu::Option_Music;
        hud->addItem(settingsPage, item);

        item = HudItem();
        item.label = french ? L"Plein \u00E9cran" : L"Full screen";
        item.title = item.label;
        item.text = french ? L"Le lanceur occupe tout l'\u00E9cran, ou s'ouvre dans une fen\u00EAtre. F11 bascule aussi." : L"The launcher fills the screen, or opens in a window. F11 switches too.";
        item.action = change;
        item.icon = Icon_Keys;
        item.option = HudMenu::Option_FullScreen;
        hud->addItem(settingsPage, item);

        item = HudItem();
        item.label = french ? L"Retour" : L"Back";
        item.title = item.label;
        item.text = french ? L"Revenir au menu principal." : L"Back to the main menu.";
        item.action = french ? L"RETOUR" : L"BACK";
        item.icon = Icon_Back;
        item.back = true;
        hud->addItem(settingsPage, item);
    }

    //Overlays, created after the menu so that they lie on top of it.
    g_keySheet = new KeySheet(env, titleFont, textFont, smallFont);
    g_keySheet->drop();
    g_screenPicker = new ScreenPicker(env, french, bigFont ? bigFont : titleFont, titleFont, textFont, smallFont,
        [](int screen, int consoleScreen) {
            saveLauncherScreen("Bridge", screen);
            saveLauncherScreen("Instruments", consoleScreen);
            showLaunchToast(g_simulatorTitle);
            launchSimulator(screen, consoleScreen);
        },
        [](int screen) {
            saveLauncherScreen("Repeater", screen);
            showLaunchToast(g_repeaterTitle);
            launchRepeater(screen);
        });
    g_screenPicker->drop();

    //The intro film, if there is one (media/intro.mp4).
    Receiver receiver;
    device->setEventReceiver(&receiver);
    const std::string introFile = VideoClip::find("media/intro");
    receiver.intro = options.introVideo && !noIntro && !introFile.empty() && intro.open(introFile, true, true, false);

    auto showMenu = [&]() {
        receiver.intro = false;
        const std::string filmFile = VideoClip::find("media/menu_background");
        if (!filmFile.empty() && film.open(filmFile, true, false, true)) { hud->setBackground(bgTex, &film); }
        if (options.menuMusic) { startMusic(); }
        hud->reveal();
    };
    if (!receiver.intro) { showMenu(); }

    irr::u32 introFade = 0;
    bool wasActive = true;
    bool filmLogged = false, musicLogged = false;

    // Render loop
    while (device->run()) {
        const irr::u32 frameStart = device->getTimer()->getRealTime();
        const bool active = device->isWindowActive();
        if (active != wasActive) {
            //Behind an exercise: no film, no music, few frames. Back in front: up to date.
            hud->setWindowActive(active);
            music.pause(!active);
            if (active && hud->item(mainPage, 0)) { hud->item(mainPage, 0)->facts = simulatorFacts(french); }
            wasActive = active;
        }
        if (device->isWindowMinimized()) {
            device->sleep(100);
            continue;
        }

        driver->beginScene(irr::video::ECBF_COLOR | irr::video::ECBF_DEPTH, irr::video::SColor(255, 0, 0, 0));
        if (receiver.intro) {
            //Skipped (key, click) or over: a short fade to black, then the menu.
            const irr::u32 now = device->getTimer()->getRealTime();
            if (introFade == 0 && (receiver.skip || intro.finished())) { introFade = now; }
            const irr::f32 fade = introFade ? irr::core::clamp((now - introFade) / 450.0f, 0.0f, 1.0f) : 0.0f;
            if (introFade) { intro.setVolume(1.0f - fade); }
            const irr::f32 prompt = intro.started() ? irr::core::clamp((intro.position() - 0.6f) / 0.5f, 0.0f, 1.0f) : 0.0f;
            hud->drawIntro(&intro, fade, prompt * (1.0f - fade));
            if (intro.failed() || fade >= 1.0f) {
                if (intro.failed()) { logLine("Intro film " + introFile + ": " + intro.error()); }
                intro.close();
                showMenu();
            }
        }
        else {
            music.update(driver);
            env->drawAll();
            //A film or music that turns out not to play: said once in launcher.log, then left alone.
            if (film.isOpen() && film.failed() && !filmLogged) {
                logLine("Menu background film: " + film.error());
                hud->setBackground(bgTex, 0);
                filmLogged = true;
            }
            if (music.isOpen() && music.failed() && !musicLogged) {
                logLine("Menu music: " + music.error());
                musicLogged = true;
            }
        }
        driver->endScene();

        //Smooth while in front (vsync, else about 120 frames a second at most), light on the CPU behind.
        if (!active && !receiver.intro) {
            device->sleep(100);
        }
        else {
            const irr::u32 spent = device->getTimer()->getRealTime() - frameStart;
            if (spent < 8) { device->sleep(8 - spent); }
        }
#ifndef _WIN32
        while (waitpid(-1, 0, WNOHANG) > 0) {} //applications started and since closed
#endif
    }

    intro.close();
    film.close();
    music.close();
    g_hud = 0;
    device->drop();
    return EXIT_SUCCESS;
}
