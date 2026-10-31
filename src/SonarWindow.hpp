/*   NAUTITECH - Simulateur de Navigation
     The sonar station: a window of its own (for a second screen) with the 360 degree search sonar,
     the fish finder, or both side by side, and their controls.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __SONARWINDOW_HPP_INCLUDED__
#define __SONARWINDOW_HPP_INCLUDED__

#include "irrlicht.h"
#include "Sonar.hpp"
#include <string>
#include <vector>

class ConsoleWindow;
class HudFont;
class SimulationModel;

//Drawn by the simulator into its own native window, like the detached radar: same OpenGL context,
//one beginScene/endScene pass per frame (at most 30 a second), mouse input only (the keyboard
//stays with the bridge view). Its place, size, full screen and mode are kept in sonarWindow.ini,
//and it opens again at the next start if it was open.
class SonarWindow
{
public:
    SonarWindow(irr::IrrlichtDevice* device, SimulationModel* model, irr::u32 instance);
    ~SonarWindow();

    //A screen of its own (launcher -sonar N, or bc5.ini sonar_screen=N): opened there at start
    void setScreen(const irr::core::rect<irr::s32>& area);
    //Opens it if it was open at the end of the last session, or if it has a screen
    void restore();
    void toggle();
    bool isOpen() const;
    //Every frame, after the bridge view's: input, the sonars, and the window's frame
    void render();
    //(X11) the main window's drawable, to switch back to; 0 if the window never opened
    const irr::video::SExposedVideoData* mainVideoData() const;
    //Saves its placement and closes (end of the simulator)
    void shutdown();
    //Frames a second drawn into the window (the driver counts them with the bridge view's)
    irr::u32 renderRate() const { return isOpen() ? rate : 0; }
    //bc5.ini sonar_window_fps
    void setFrameRate(irr::u32 fps) { if (fps > 0) { frameMs = 1000 / fps; } }

private:
    enum Mode { MODE_360 = 0, MODE_SOUNDER, MODE_BOTH };
    enum Action {
        A_NONE = 0, A_MODE_360, A_MODE_SOUNDER, A_MODE_BOTH, A_FULLSCREEN, A_CLOSE,
        A_RANGE_DOWN, A_RANGE_UP, A_TILT_DOWN, A_TILT_UP, A_GAIN360_DOWN, A_GAIN360_UP, A_NORTH_UP, A_HEAD_UP, A_CLEAR_MARKER,
        A_SRANGE_DOWN, A_SRANGE_UP, A_SAUTO, A_SGAIN_DOWN, A_SGAIN_UP, A_FREQ50, A_FREQ200, A_SCROLL_SLOWER, A_SCROLL_FASTER,
        A_CLUTTER_DOWN, A_CLUTTER_UP, A_FREQBOTH, A_WHITELINE_ON, A_WHITELINE_OFF, A_BG_PREV, A_BG_NEXT, A_COL_PREV, A_COL_NEXT,
        A_FADE_DOWN, A_FADE_UP, A_HEAVE_ON, A_HEAVE_OFF, A_HELP, A_HELP_TAB0, A_HELP_TAB1, A_HELP_TAB2,
        A_TVG360_DOWN, A_TVG360_UP, A_LEVEL360_DOWN, A_LEVEL360_UP, A_VOLUME_DOWN, A_VOLUME_UP,
        A_STVG_DOWN, A_STVG_UP, A_SLEVEL_DOWN, A_SLEVEL_UP,
        A_LISTEN_AHEAD, A_LISTEN_MARKER, A_LISTEN_SCAN, A_ALARM_ON, A_ALARM_OFF,
        A_ALARM_MIN_DOWN, A_ALARM_MIN_UP, A_ALARM_MAX_DOWN, A_ALARM_MAX_UP
    };
    struct Button {
        irr::core::rect<irr::f32> r;
        Action action;
        std::wstring label;
        bool selected;
    };

    void open();
    void close();
    void savePlacement(bool isOpen);
    void loadFonts(irr::f32 k);
    void freeFonts();

    void layout(irr::u32 w, irr::u32 h);
    void handleInput();
    void act(Action a);
    void setMarkerAt(irr::core::position2di p);

    void updateTexture360();
    void updateTextureEcho(int f);
    void draw(irr::u32 w, irr::u32 h);
    void drawHeader();
    void drawPanel();
    void draw360();
    void drawSounder();
    void addButton(const irr::core::rect<irr::f32>& r, Action a, const std::wstring& label, bool selected = false);
    void addHeaderButtons();
    void drawHelp();
    void drawButtons();
    irr::f32 panelRow(irr::f32 y, const std::wstring& label, const std::wstring& value, Action down, Action up,
        const wchar_t* downLabel = L"-", const wchar_t* upLabel = L"+");
    //Colours: background and echo palette chosen, in steps as a real sonar shows them
    void rebuildPalette();
    bool brightBackground() const { return bgIndex == 2; }
    irr::video::SColor overlayColour(irr::u32 alpha) const;
    void drawEchogram(int f, const irr::core::rect<irr::f32>& r);
    void line(irr::f32 x0, irr::f32 y0, irr::f32 x1, irr::f32 y1, irr::video::SColor c);
    void circle(irr::f32 cx, irr::f32 cy, irr::f32 r, irr::video::SColor c, int segments = 120);
    irr::video::ITexture* makeTexture(const char* name);

    irr::IrrlichtDevice* device;
    irr::video::IVideoDriver* driver;
    SimulationModel* model;
    Sonar sonar;
    ConsoleWindow* window;
    bool windowUsed;
    irr::u32 instance;
    std::string placementFile;

    //Placement
    irr::s32 placeX, placeY;
    irr::u32 placeW, placeH;
    bool placeFullScreen;
    bool onScreen;
    irr::core::rect<irr::s32> screenArea;

    //State
    Mode mode;
    bool headUp;
    bool markerOn;
    irr::f32 markerBearing;   //true, degrees
    irr::f32 markerRange;     //slant, metres
    irr::core::position2di mouse;
    bool mouseInside;
    irr::u32 lastRealMs;
    irr::u32 lastRenderMs;
    irr::u32 frameMs;
    bool fullScreenRequest;
    //The guide (AIDE), over the pictures
    bool helpOpen = false;
    int helpTab = 0;
    irr::f32 helpScroll = 0.0f;
    irr::f32 helpContentH = 0.0f;
    irr::core::rect<irr::f32> helpArea, helpText;
    irr::u32 renderCount, rateStartMs, rate;

    //Layout of the current frame
    irr::f32 k;               //scale (1 at a height of 1000 pixels)
    irr::f32 rk = 1;          //the settings rows' height scale: k, or less when they would not all fit
    int echoVolume = 0;       //the loudspeaker, 0 (off, the default) .. 10
    int listenMode = 0;       //its listening beam: Sonar::LISTEN_AHEAD, _MARKER or _SCAN
    irr::core::rect<irr::f32> header, panel, area360, areaEcho, picEcho, scaleEcho, ascopeEcho;
    irr::core::vector2df centre360;
    irr::f32 radius360;
    std::vector<Button> buttons;

    //Pictures
    enum { TEX = 512 };
    irr::video::ITexture* tex360;
    irr::video::ITexture* texEcho[2];
    irr::u32 serial360Drawn, serialEchoDrawn[2];
    irr::u32 paletteSerial, palette360Drawn, paletteEchoDrawn[2];
    int freqMode;             //0: 50 kHz, 1: 200 kHz, 2: both side by side
    bool whiteLine;           //the bottom's first edge in white (fish close to the bottom stand out)
    int bgIndex;              //background: dark blue, black, white, light blue
    int colourIndex;          //echoes: 16 colours, 8 colours, amber, grey
    int intensity = 10;       //echo colours, 2..10 tenths: lower fades them towards the background
    int colourLevels;
    irr::f32 headingDrawn;
    irr::u32 lastTex360Ms;
    std::vector<irr::u16> lutAngle;   //tenths of a degree clockwise from the top, 0xFFFF outside the disc
    std::vector<irr::u32> lutBin;     //range in 1/256 of a bin (for blending between bins)
    std::vector<irr::u32> palette;    //echo strength to colour (A8R8G8B8)

    //Fonts
    irr::f32 fontScale;
    HudFont* fontTitle;
    HudFont* fontText;
    HudFont* fontSmall;
    HudFont* fontBig;
};

#endif
