//The launcher's main screen, full screen like a game menu: an animated background (a looping film,
//or the launcher picture slowly moving), a HUD frame, the menu on the left (mouse or keyboard, with
//sounds), the details of the item selected on the right, a radar, and the keys at the bottom.
//Items open applications (through the action callback), sub-pages, or switch launcher options.

#ifndef __LAUNCHER_HUDMENU_HPP_INCLUDED__
#define __LAUNCHER_HUDMENU_HPP_INCLUDED__

#include "irrlicht.h"
#include "LauncherDraw.hpp"
#include <functional>
#include <string>
#include <utility>
#include <vector>

class HudFont;
class UiSounds;
class VideoClip;

//Launcher preferences, in the user folder (launcher.ini).
struct LauncherOptions
{
    bool introVideo, uiSounds, menuMusic, fullScreen;
    LauncherOptions() : introVideo(true), uiSounds(true), menuMusic(true), fullScreen(true) {}
    void load();
    bool save() const;
};

struct HudItem
{
    irr::s32 id;                     //action id passed to the callback
    std::wstring label;              //in the menu
    std::wstring title;              //at the top of the details
    std::wstring text;               //description
    std::vector<std::wstring> tags;  //short facts, as chips
    std::vector<std::pair<std::wstring, std::wstring> > facts; //label, value
    std::wstring action;             //what Enter does ("LANCER", "OUVRIR", ...)
    LauncherIcon icon;
    bool locked;                     //the tool asks for the administrator password
    bool launches;                   //starts an application (launch sound)
    int page;                        //>= 0: opens this page
    int option;                      //>= 0: switches this launcher option
    bool back;                       //back to the main page
    bool quit;                       //asks, then quits

    HudItem() : id(-1), icon(Icon_Helm), locked(false), launches(false), page(-1), option(-1), back(false), quit(false) {}
};

//Ids the menu sends for its own controls.
const irr::s32 HUD_MINIMISE = -100;
const irr::s32 HUD_QUIT = -101;
const irr::s32 HUD_TOGGLE_FULLSCREEN = -102; //F11

class HudMenu : public irr::gui::IGUIElement
{
public:
    enum Option { Option_Intro, Option_Sounds, Option_Music, Option_FullScreen, OptionCount };
    typedef std::function<void(const HudItem&)> ActionFn;
    typedef std::function<bool()> BlockedFn;

    //fontFolder: the TrueType faces (Barlow Condensed). fallback: bitmap font if they are missing.
    //blocked: true while another screen (key sheet, screen picker) is open over the menu.
    HudMenu(irr::gui::IGUIEnvironment* env, bool french, const std::string& fontFolder, irr::gui::IGUIFont* fallback,
        UiSounds* sounds, LauncherOptions* options, ActionFn onAction, BlockedFn blocked);
    virtual ~HudMenu();

    int addPage(const std::wstring& heading);
    void addItem(int page, const HudItem& item);
    HudItem* item(int page, size_t index);

    void setBackground(irr::video::ITexture* picture, VideoClip* film);
    void setStatus(const std::wstring& station, const std::wstring& version);
    void setWindowControls(bool on) { windowControls = on; }

    //The intro film full screen (black bars as needed), dimmed by fadeOut (0..1), with the prompt to
    //skip it (prompt: its opacity) and its progress. The menu is not shown meanwhile.
    void drawIntro(VideoClip* clip, irr::f32 fadeOut, irr::f32 prompt);
    void reveal();                              //entry animation (after the intro film)
    void showToast(const std::wstring& title);  //"launching ..."
    void setWindowActive(bool active);
    bool isAnimating() const;

    virtual void draw();
    virtual bool OnEvent(const irr::SEvent& event);

private:
    struct Page {
        std::wstring heading;
        std::vector<HudItem> items;
    };
    struct Fonts {
        HudFont* brand; HudFont* sub; HudFont* heading; HudFont* menu; HudFont* index; HudFont* title;
        HudFont* text; HudFont* small; HudFont* tiny; HudFont* quit;
    };

    void layout(const irr::core::dimension2du& size);
    void freeFonts();
    irr::f32 seconds() const;

    void select(int index, bool sound);
    void activate(int index);
    void goToPage(int page, bool forward);
    void back();
    void closeQuitPrompt(bool quit);
    int rowAt(const irr::core::vector2df& p) const;
    irr::core::rect<irr::f32> rowRect(int index) const;
    std::wstring optionText(int option) const;

    void drawBackground(irr::video::IVideoDriver* driver, irr::f32 t, irr::f32 r);
    void drawAtmosphere(irr::video::IVideoDriver* driver, irr::f32 t, irr::f32 r);
    void drawFrame(irr::video::IVideoDriver* driver, irr::f32 r);
    void drawHeader(irr::video::IVideoDriver* driver, irr::f32 r);
    void drawMenu(irr::video::IVideoDriver* driver, irr::f32 t, irr::f32 r);
    void drawDetails(irr::video::IVideoDriver* driver, irr::f32 t, irr::f32 r);
    void drawRadar(irr::video::IVideoDriver* driver, irr::f32 t, irr::f32 r);
    void drawFooter(irr::video::IVideoDriver* driver, irr::f32 r);
    void drawToast(irr::video::IVideoDriver* driver, irr::f32 t);
    void drawQuitPrompt(irr::video::IVideoDriver* driver, irr::f32 t);
    //A key cap and what it does; returns the x after the text.
    irr::f32 drawKeyHint(irr::video::IVideoDriver* driver, const std::wstring& key, const std::wstring& text, irr::f32 x, irr::f32 cy, irr::f32 alpha);

    bool french;
    std::string fontFolder;
    irr::gui::IGUIFont* fallback;
    UiSounds* sounds;
    LauncherOptions* options;
    ActionFn onAction;
    BlockedFn blocked;

    std::vector<Page> pages;
    int page, selected, pressedRow;
    irr::f32 selectionY;            //animated highlight position (row units)
    irr::f32 lastSeconds;

    irr::video::ITexture* picture;
    VideoClip* film;
    std::wstring station, version;
    bool windowControls, windowActive;

    //Times (seconds) of the animations
    irr::f32 revealAt, pageAt, selectAt, activateAt, toastAt, quitAt;
    bool revealed, pageForward;
    std::wstring toastText;
    bool quitOpen;
    int quitChoice;                  //0: quit, 1: cancel
    bool enterHeld;                  //Enter or Space went down here: it acts when released

    irr::core::vector2df mouse;
    irr::core::dimension2du laidOut;
    Fonts fonts;
    irr::f32 k;                      //scale: screen height / 1080
    irr::f32 mx, headerH, footerY, menuX, menuTop, rowH, rowW;
    irr::core::rect<irr::f32> panel, minimiseButton, quitPanel, quitButtons[2];
    irr::core::vector2df radarCentre;
    irr::f32 radarRadius;
    long long startedAt;
};

#endif
