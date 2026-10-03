#include "HudMenu.hpp"
#include "HudText.hpp"
#include "UiSound.hpp"
#include "VideoClip.hpp"
#include "../Utilities.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <fstream>

#ifdef _WIN32
#include <direct.h> //_mkdir
#else
#include <sys/stat.h> //mkdir
#endif

namespace {

const irr::f32 kPi = 3.14159265f;

irr::f32 clamp01(irr::f32 v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

irr::f32 easeOut(irr::f32 t)
{
    const irr::f32 u = 1.0f - clamp01(t);
    return 1.0f - u * u * u;
}

irr::video::SColor fade(irr::video::SColor c, irr::f32 a)
{
    c.setAlpha((irr::u32)(c.getAlpha() * clamp01(a) + 0.5f));
    return c;
}

irr::core::rect<irr::s32> toInt(const irr::core::rect<irr::f32>& r)
{
    return irr::core::rect<irr::s32>((irr::s32)std::floor(r.UpperLeftCorner.X), (irr::s32)std::floor(r.UpperLeftCorner.Y),
        (irr::s32)std::ceil(r.LowerRightCorner.X), (irr::s32)std::ceil(r.LowerRightCorner.Y));
}

//HUD colours
const irr::video::SColor cAccent(255, 64, 160, 255);
const irr::video::SColor cAccentSoft(255, 132, 200, 255);
const irr::video::SColor cWhite(255, 255, 255, 255);
const irr::video::SColor cText(255, 236, 243, 250);
const irr::video::SColor cDim(255, 172, 192, 214);
const irr::video::SColor cFaint(255, 120, 142, 168);
const irr::video::SColor cNavy(255, 5, 13, 27);
const irr::video::SColor cAmber(255, 255, 196, 92);
const irr::video::SColor cDanger(255, 232, 76, 70);

std::string optionsFile()
{
    return Utilities::getUserDir() + "launcher.ini";
}

//Specks of light drifting up the screen.
struct Speck {
    irr::f32 x, y, vx, vy, size, phase, rate;
};
std::vector<Speck> specks;

}

//-------------------------------------------------------------------------------------------------
//Options
//-------------------------------------------------------------------------------------------------

void LauncherOptions::load()
{
    std::ifstream in(optionsFile().c_str());
    std::string line;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) { continue; }
        std::string key = Utilities::trim(line.substr(0, eq));
        const std::string value = Utilities::trim(Utilities::trim(line.substr(eq + 1)), "\"");
        Utilities::to_lower(key);
        const bool on = (value != "0");
        if (key == "introvideo") { introVideo = on; }
        else if (key == "uisounds") { uiSounds = on; }
        else if (key == "menumusic") { menuMusic = on; }
        else if (key == "fullscreen") { fullScreen = on; }
    }
}

bool LauncherOptions::save() const
{
    const std::string dirs[2] = { Utilities::getUserDirBase(), Utilities::getUserDir() };
    for (int d = 0; d < 2; d++) {
        std::string dir = dirs[d];
        if (dir.size() > 1 && !Utilities::pathExists(dir)) {
            dir.erase(dir.size() - 1);
#ifdef _WIN32
            _mkdir(dir.c_str());
#else
            mkdir(dir.c_str(), 0755);
#endif
        }
    }
    std::ofstream out(optionsFile().c_str(), std::ios::trunc);
    out << "IntroVideo=" << (introVideo ? 1 : 0) << "\n";
    out << "UiSounds=" << (uiSounds ? 1 : 0) << "\n";
    out << "MenuMusic=" << (menuMusic ? 1 : 0) << "\n";
    out << "FullScreen=" << (fullScreen ? 1 : 0) << "\n";
    return out.good();
}

//-------------------------------------------------------------------------------------------------
//Menu
//-------------------------------------------------------------------------------------------------

HudMenu::HudMenu(irr::gui::IGUIEnvironment* env, bool french, const std::string& fontFolder, irr::gui::IGUIFont* fallback,
    UiSounds* sounds, LauncherOptions* options, ActionFn onAction, BlockedFn blocked)
    : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, env, env->getRootGUIElement(), -1, irr::core::rect<irr::s32>(0, 0, 10, 10)),
    french(french), fontFolder(fontFolder), fallback(fallback), sounds(sounds), options(options), onAction(onAction), blocked(blocked),
    page(0), selected(0), pressedRow(-1), selectionY(0), lastSeconds(0), picture(0), film(0), windowControls(false), windowActive(true),
    revealAt(0), pageAt(-10), selectAt(-10), activateAt(-10), toastAt(-10), quitAt(-10), revealed(false), pageForward(true),
    quitOpen(false), quitChoice(0), enterHeld(false), mouse(-1000, -1000), laidOut(0, 0), k(1), mx(0), headerH(0), footerY(0), menuX(0), menuTop(0),
    rowH(0), rowW(0), radarRadius(0)
{
    std::memset(&fonts, 0, sizeof(fonts));
    startedAt = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (specks.empty()) {
        irr::u32 seed = 0x9E3779B9u;
        auto rnd = [&]() {
            seed = seed * 1664525u + 1013904223u;
            return ((seed >> 8) & 0xFFFF) / 65535.0f;
        };
        for (int i = 0; i < 70; i++) {
            Speck s;
            s.x = rnd();
            s.y = rnd();
            s.vx = (rnd() - 0.3f) * 0.006f;
            s.vy = 0.004f + rnd() * 0.012f;
            s.size = 0.8f + rnd() * rnd() * 2.6f;
            s.phase = rnd() * 6.28f;
            s.rate = 0.4f + rnd() * 1.6f;
            specks.push_back(s);
        }
    }
    setVisible(false);
}

HudMenu::~HudMenu()
{
    freeFonts();
}

void HudMenu::freeFonts()
{
    HudFont** all = (HudFont**)&fonts;
    for (size_t i = 0; i < sizeof(fonts) / sizeof(HudFont*); i++) {
        delete all[i];
        all[i] = 0;
    }
}

irr::f32 HudMenu::seconds() const
{
    const long long now = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return (now - startedAt) / 1000.0f;
}

int HudMenu::addPage(const std::wstring& heading)
{
    Page p;
    p.heading = heading;
    pages.push_back(p);
    return (int)pages.size() - 1;
}

void HudMenu::addItem(int pageIndex, const HudItem& item)
{
    if (pageIndex >= 0 && pageIndex < (int)pages.size()) { pages[pageIndex].items.push_back(item); }
    laidOut = irr::core::dimension2du(0, 0);
}

HudItem* HudMenu::item(int pageIndex, size_t index)
{
    if (pageIndex < 0 || pageIndex >= (int)pages.size() || index >= pages[pageIndex].items.size()) { return 0; }
    return &pages[pageIndex].items[index];
}

void HudMenu::setBackground(irr::video::ITexture* pictureTexture, VideoClip* filmClip)
{
    picture = pictureTexture;
    film = filmClip;
}

void HudMenu::setStatus(const std::wstring& stationName, const std::wstring& versionText)
{
    station = stationName;
    version = versionText;
}

void HudMenu::reveal()
{
    revealed = true;
    revealAt = seconds();
    pageAt = revealAt;
    selectAt = revealAt;
    setVisible(true);
    Environment->setFocus(this);
    if (sounds) { sounds->play(UiSounds::Open); }
}

void HudMenu::drawIntro(VideoClip* clip, irr::f32 fadeOut, irr::f32 prompt)
{
    irr::video::IVideoDriver* driver = Environment->getVideoDriver();
    const irr::core::dimension2du size = driver->getScreenSize();
    if (size != laidOut) { layout(size); }
    const irr::f32 W = (irr::f32)size.Width, H = (irr::f32)size.Height;
    driver->draw2DRectangle(irr::video::SColor(255, 0, 0, 0), irr::core::rect<irr::s32>(0, 0, (irr::s32)W, (irr::s32)H));
    irr::video::ITexture* tex = clip ? clip->update(driver) : 0;
    if (tex && clip->started()) {
        const irr::core::dimension2du film = clip->size();
        if (film.Width > 0 && film.Height > 0) {
            const irr::f32 scale = std::min(W / film.Width, H / film.Height);
            const irr::f32 w = film.Width * scale, h = film.Height * scale;
            const irr::core::rect<irr::s32> dest((irr::s32)((W - w) * 0.5f), (irr::s32)((H - h) * 0.5f), (irr::s32)((W + w) * 0.5f), (irr::s32)((H + h) * 0.5f));
            const irr::u32 v = (irr::u32)(255 * clamp01(clip->position() / 0.35f) * (1.0f - clamp01(fadeOut)));
            const irr::video::SColor shade[4] = { irr::video::SColor(255, v, v, v), irr::video::SColor(255, v, v, v),
                irr::video::SColor(255, v, v, v), irr::video::SColor(255, v, v, v) };
            driver->getMaterial2D().TextureLayer[0].BilinearFilter = true;
            driver->getMaterial2D().TextureLayer[0].TrilinearFilter = false;
            driver->enableMaterial2D(true);
            driver->draw2DImage(tex, dest, irr::core::rect<irr::s32>(0, 0, (irr::s32)film.Width, (irr::s32)film.Height), 0, shade, false);
            driver->enableMaterial2D(false);
        }
        //Progress, a thin line at the bottom
        const irr::f32 length = clip->duration();
        if (length > 0) {
            const irr::f32 p = clamp01(clip->position() / length);
            driver->draw2DRectangle(fade(irr::video::SColor(150, 64, 160, 255), 1.0f - fadeOut),
                irr::core::rect<irr::s32>(0, (irr::s32)(H - std::max(2.0f, 3 * k)), (irr::s32)(W * p), (irr::s32)H));
        }
    }
    if (prompt > 0.01f) {
        const std::wstring key = french ? L"\u00C9CHAP" : L"ESC";
        const std::wstring text = french ? L"PASSER L'INTRODUCTION" : L"SKIP INTRO";
        const irr::f32 width = std::max(30 * k, fonts.tiny->width(key, 1.5f * k) + 18 * k) + 12 * k + fonts.tiny->width(text, 2.5f * k);
        drawKeyHint(driver, key, text, W - mx - width, H - 58 * k, prompt);
    }
}

void HudMenu::showToast(const std::wstring& title)
{
    toastText = title;
    toastAt = seconds();
}

void HudMenu::setWindowActive(bool active)
{
    windowActive = active;
    if (film) { film->pause(!active); }
}

bool HudMenu::isAnimating() const
{
    return true; //the radar turns, the background moves
}

//-------------------------------------------------------------------------------------------------
//Layout
//-------------------------------------------------------------------------------------------------

void HudMenu::layout(const irr::core::dimension2du& size)
{
    laidOut = size;
    const irr::f32 W = (irr::f32)size.Width, H = (irr::f32)size.Height;
    setRelativePosition(irr::core::rect<irr::s32>(0, 0, (irr::s32)W, (irr::s32)H));
    k = irr::core::clamp(H / 1080.0f, 0.5f, 3.0f);
    if (W < H * 1.25f) { k = irr::core::clamp(std::min(k, W / 1350.0f), 0.5f, 3.0f); }

    freeFonts();
    irr::video::IVideoDriver* driver = Environment->getVideoDriver();
    const std::string bold = fontFolder + "BarlowCondensed-Bold.ttf";
    const std::string semi = fontFolder + "BarlowCondensed-SemiBold.ttf";
    const std::string medium = fontFolder + "BarlowCondensed-Medium.ttf";
    fonts.brand = new HudFont(driver, bold, 46 * k, fallback);
    fonts.sub = new HudFont(driver, medium, 20 * k, fallback);
    fonts.heading = new HudFont(driver, semi, 23 * k, fallback);
    fonts.menu = new HudFont(driver, semi, 52 * k, fallback);
    fonts.index = new HudFont(driver, medium, 22 * k, fallback);
    fonts.title = new HudFont(driver, bold, 72 * k, fallback);
    fonts.text = new HudFont(driver, medium, 30 * k, fallback);
    fonts.small = new HudFont(driver, medium, 24 * k, fallback);
    fonts.tiny = new HudFont(driver, semi, 19 * k, fallback);
    fonts.quit = new HudFont(driver, bold, 56 * k, fallback);

    mx = std::floor(std::max(28.0f, std::min(96 * k, W * 0.05f)));
    headerH = std::floor(150 * k);
    footerY = std::floor(H - 78 * k);
    menuX = mx + std::floor(8 * k);
    rowH = std::floor(66 * k);
    rowW = std::floor(std::min(640 * k, W * 0.44f));
    size_t most = 1;
    for (size_t p = 0; p < pages.size(); p++) { most = std::max(most, pages[p].items.size()); }
    menuTop = std::floor(irr::core::clamp(H * 0.29f, headerH + 96 * k, std::max(headerH + 96 * k, footerY - 30 * k - most * rowH)));

    //Details on the right, the radar under them
    radarRadius = std::floor(76 * k);
    radarCentre = irr::core::vector2df(W - mx - radarRadius - 10 * k, footerY - 40 * k - radarRadius);
    const irr::f32 panelLeft = std::max(menuX + rowW + 64 * k, W * 0.55f);
    panel = irr::core::rect<irr::f32>(panelLeft, menuTop - 52 * k, W - mx, radarCentre.Y - radarRadius - 30 * k);
    if (panel.getWidth() < 360 * k || panel.getHeight() < 260 * k) {
        panel = irr::core::rect<irr::f32>(panelLeft, menuTop - 8 * k, W - mx, footerY - 30 * k);
        radarRadius = 0;
    }
    if (panel.getWidth() < 300 * k) { panel = irr::core::rect<irr::f32>(0, 0, 0, 0); }
    if (radarCentre.X - radarRadius < menuX + rowW + 30 * k) { radarRadius = 0; }

    minimiseButton = irr::core::rect<irr::f32>(W - mx - 46 * k, 52 * k, W - mx, 98 * k);
    const irr::f32 qw = std::min(680 * k, W - 2 * mx), qh = 300 * k;
    quitPanel = irr::core::rect<irr::f32>((W - qw) * 0.5f, (H - qh) * 0.5f, (W + qw) * 0.5f, (H + qh) * 0.5f);
    const irr::f32 bw = 230 * k, bh = 60 * k, by = quitPanel.LowerRightCorner.Y - 40 * k - bh;
    quitButtons[0] = irr::core::rect<irr::f32>(W * 0.5f - bw - 10 * k, by, W * 0.5f - 10 * k, by + bh);
    quitButtons[1] = irr::core::rect<irr::f32>(W * 0.5f + 10 * k, by, W * 0.5f + bw + 10 * k, by + bh);
}

irr::core::rect<irr::f32> HudMenu::rowRect(int index) const
{
    return irr::core::rect<irr::f32>(menuX - 24 * k, menuTop + index * rowH, menuX + rowW, menuTop + (index + 1) * rowH);
}

int HudMenu::rowAt(const irr::core::vector2df& p) const
{
    if (page < 0 || page >= (int)pages.size()) { return -1; }
    for (size_t i = 0; i < pages[page].items.size(); i++) {
        if (rowRect((int)i).isPointInside(p)) { return (int)i; }
    }
    return -1;
}

std::wstring HudMenu::optionText(int option) const
{
    bool on = false;
    if (options) {
        if (option == Option_Intro) { on = options->introVideo; }
        else if (option == Option_Sounds) { on = options->uiSounds; }
        else if (option == Option_Music) { on = options->menuMusic; }
        else if (option == Option_FullScreen) { on = options->fullScreen; }
    }
    return on ? (french ? L"OUI" : L"ON") : (french ? L"NON" : L"OFF");
}

//-------------------------------------------------------------------------------------------------
//Actions
//-------------------------------------------------------------------------------------------------

void HudMenu::select(int index, bool sound)
{
    if (page < 0 || page >= (int)pages.size() || pages[page].items.empty()) { return; }
    const int n = (int)pages[page].items.size();
    index = ((index % n) + n) % n;
    if (index == selected) { return; }
    selected = index;
    selectAt = seconds();
    if (sound && sounds) { sounds->play(UiSounds::Hover); }
}

void HudMenu::activate(int index)
{
    if (page < 0 || page >= (int)pages.size() || index < 0 || index >= (int)pages[page].items.size()) { return; }
    const HudItem item = pages[page].items[index];
    activateAt = seconds();
    if (item.back) {
        back();
        return;
    }
    if (item.quit) {
        if (sounds) { sounds->play(UiSounds::Select); }
        quitOpen = true;
        quitChoice = 0;
        quitAt = seconds();
        return;
    }
    if (item.page >= 0) {
        if (sounds) { sounds->play(UiSounds::Select); }
        goToPage(item.page, true);
        return;
    }
    if (item.option >= 0 && options) {
        if (item.option == Option_Intro) { options->introVideo = !options->introVideo; }
        else if (item.option == Option_Sounds) { options->uiSounds = !options->uiSounds; }
        else if (item.option == Option_Music) { options->menuMusic = !options->menuMusic; }
        else if (item.option == Option_FullScreen) { options->fullScreen = !options->fullScreen; }
        options->save();
        if (sounds) {
            sounds->setEnabled(options->uiSounds);
            sounds->play(UiSounds::Select);
        }
        if (onAction) { onAction(item); }
        return;
    }
    if (sounds) { sounds->play(item.launches ? UiSounds::Launch : UiSounds::Select); }
    if (onAction) { onAction(item); }
}

void HudMenu::goToPage(int target, bool forward)
{
    static int mainSelection = 0;
    if (forward) { mainSelection = selected; }
    page = target;
    selected = forward ? 0 : mainSelection;
    selectionY = (irr::f32)selected;
    pageAt = seconds();
    selectAt = pageAt;
    pageForward = forward;
}

void HudMenu::back()
{
    if (page != 0) {
        if (sounds) { sounds->play(UiSounds::Back); }
        goToPage(0, false);
        return;
    }
    if (sounds) { sounds->play(UiSounds::Back); }
    quitOpen = true;
    quitChoice = 0;
    quitAt = seconds();
}

void HudMenu::closeQuitPrompt(bool quit)
{
    quitOpen = false;
    if (quit) {
        if (sounds) { sounds->play(UiSounds::Select); }
        HudItem q;
        q.id = HUD_QUIT;
        if (onAction) { onAction(q); }
    }
    else if (sounds) {
        sounds->play(UiSounds::Back);
    }
}

bool HudMenu::OnEvent(const irr::SEvent& event)
{
    if (!IsVisible) { return IGUIElement::OnEvent(event); }
    if (event.EventType == irr::EET_GUI_EVENT) { return false; }
    if (blocked && blocked()) { return false; }
    if (!revealed) { return true; }

    if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
        const irr::SEvent::SMouseInput& m = event.MouseInput;
        mouse = irr::core::vector2df((irr::f32)m.X, (irr::f32)m.Y);
        if (quitOpen) {
            for (int i = 0; i < 2; i++) {
                if (quitButtons[i].isPointInside(mouse) && quitChoice != i) {
                    quitChoice = i;
                    if (sounds) { sounds->play(UiSounds::Hover); }
                }
            }
            if (m.Event == irr::EMIE_LMOUSE_LEFT_UP) {
                if (quitButtons[0].isPointInside(mouse)) { closeQuitPrompt(true); }
                else if (quitButtons[1].isPointInside(mouse) || !quitPanel.isPointInside(mouse)) { closeQuitPrompt(false); }
            }
            if (m.Event == irr::EMIE_RMOUSE_LEFT_UP) { closeQuitPrompt(false); }
            return true;
        }
        const int row = rowAt(mouse);
        switch (m.Event) {
        case irr::EMIE_MOUSE_MOVED:
            if (row >= 0) { select(row, true); }
            break;
        case irr::EMIE_LMOUSE_PRESSED_DOWN:
            pressedRow = row;
            if (row < 0 && windowControls && minimiseButton.isPointInside(mouse)) { pressedRow = -2; }
            break;
        case irr::EMIE_LMOUSE_LEFT_UP:
            if (row >= 0 && row == pressedRow) { activate(row); }
            else if (pressedRow == -2 && minimiseButton.isPointInside(mouse) && onAction) {
                HudItem m2;
                m2.id = HUD_MINIMISE;
                onAction(m2);
            }
            pressedRow = -1;
            break;
        case irr::EMIE_RMOUSE_LEFT_UP:
            back();
            break;
        case irr::EMIE_MOUSE_WHEEL:
            select(selected + (m.Wheel > 0 ? -1 : 1), true);
            break;
        default:
            break;
        }
        return true;
    }

    if (event.EventType == irr::EET_KEY_INPUT_EVENT) {
        const irr::SEvent::SKeyInput& key = event.KeyInput;
        //Enter and Space act when released, like buttons: the screen opened by a press (screen picker,
        //key sheet) must not receive the release and act on it in turn.
        if (key.Key == irr::KEY_RETURN || key.Key == irr::KEY_SPACE) {
            if (key.PressedDown) {
                enterHeld = true;
                return true;
            }
            if (!enterHeld) { return true; }
            enterHeld = false;
            if (quitOpen) { closeQuitPrompt(quitChoice == 0); }
            else { activate(selected); }
            return true;
        }
        if (!key.PressedDown) { return true; }
        if (quitOpen) {
            switch (key.Key) {
            case irr::KEY_LEFT: case irr::KEY_RIGHT: case irr::KEY_UP: case irr::KEY_DOWN: case irr::KEY_TAB:
                quitChoice = 1 - quitChoice;
                if (sounds) { sounds->play(UiSounds::Hover); }
                break;
            case irr::KEY_ESCAPE: case irr::KEY_BACK:
                closeQuitPrompt(false);
                break;
            default:
                break;
            }
            return true;
        }
        switch (key.Key) {
        case irr::KEY_UP: select(selected - 1, true); break;
        case irr::KEY_DOWN: select(selected + 1, true); break;
        case irr::KEY_HOME: select(0, true); break;
        case irr::KEY_END: if (!pages.empty()) { select((int)pages[page].items.size() - 1, true); } break;
        case irr::KEY_RIGHT:
            if (item(page, (size_t)selected) && item(page, (size_t)selected)->page >= 0) { activate(selected); }
            break;
        case irr::KEY_ESCAPE: case irr::KEY_BACK: case irr::KEY_LEFT: back(); break;
        case irr::KEY_F11:
            if (onAction) {
                HudItem f;
                f.id = HUD_TOGGLE_FULLSCREEN;
                onAction(f);
            }
            break;
        default:
            break;
        }
        return true;
    }
    return IGUIElement::OnEvent(event);
}

//-------------------------------------------------------------------------------------------------
//Drawing
//-------------------------------------------------------------------------------------------------

void HudMenu::draw()
{
    if (!IsVisible) { return; }
    irr::video::IVideoDriver* driver = Environment->getVideoDriver();
    const irr::core::dimension2du size = driver->getScreenSize();
    if (size != laidOut) { layout(size); }
    //The menu takes the keys back when the screens drawn over it close.
    if (blocked && !blocked() && Environment->getFocus() != this) { Environment->setFocus(this); }

    const irr::f32 t = seconds();
    const irr::f32 dt = irr::core::clamp(t - lastSeconds, 0.0f, 0.1f);
    lastSeconds = t;
    const irr::f32 r = revealed ? t - revealAt : 0.0f;
    selectionY += ((irr::f32)selected - selectionY) * (1.0f - std::exp(-dt * 18.0f));

    drawBackground(driver, t, r);
    drawAtmosphere(driver, t, r);
    drawFrame(driver, r);
    drawHeader(driver, r);
    drawMenu(driver, t, r);
    drawDetails(driver, t, r);
    drawRadar(driver, t, r);
    drawFooter(driver, r);
    drawToast(driver, t);
    if (r < 1.0f) {
        //Out of black (the intro film ends on black)
        driver->draw2DRectangle(irr::video::SColor((irr::u32)(255 * (1.0f - easeOut(r / 0.9f))), 0, 0, 0),
            irr::core::rect<irr::s32>(0, 0, (irr::s32)size.Width, (irr::s32)size.Height));
    }
    if (quitOpen) { drawQuitPrompt(driver, t); }
    IGUIElement::draw();
}

void HudMenu::drawBackground(irr::video::IVideoDriver* driver, irr::f32 t, irr::f32 r)
{
    const irr::f32 W = (irr::f32)laidOut.Width, H = (irr::f32)laidOut.Height;
    driver->draw2DRectangle(cNavy, irr::core::rect<irr::s32>(0, 0, (irr::s32)W, (irr::s32)H));
    irr::video::ITexture* tex = 0;
    irr::core::dimension2du source(0, 0);
    if (film) {
        tex = film->update(driver);
        if (tex && film->started()) { source = film->size(); } else { tex = 0; }
    }
    const bool still = (tex == 0);
    if (still && picture) {
        tex = picture;
        source = picture->getOriginalSize();
    }
    if (!tex || source.Width == 0 || source.Height == 0) { return; }

    //Cover the screen; a still picture drifts and breathes slowly, and follows the mouse a little.
    const irr::f32 breathe = still ? 0.045f + 0.035f * (0.5f - 0.5f * std::cos(2 * kPi * t / 46.0f)) : 0.015f;
    const irr::f32 zoom = 1.0f + breathe + 0.10f * (1.0f - easeOut(r / 1.8f));
    const irr::f32 mouseX = (mouse.X < -100) ? 0.5f : mouse.X / W, mouseY = (mouse.Y < -100) ? 0.5f : mouse.Y / H;
    irr::f32 panX = (mouseX - 0.5f) * 18 * k, panY = (mouseY - 0.5f) * 10 * k;
    if (still) {
        panX += 22 * k * std::sin(2 * kPi * t / 61.0f);
        panY += 12 * k * std::sin(2 * kPi * t / 53.0f);
    }
    const irr::f32 sw = (irr::f32)source.Width, sh = (irr::f32)source.Height;
    const irr::f32 scale = std::max(W / sw, H / sh) * zoom;
    const irr::f32 visW = W / scale, visH = H / scale;
    const irr::f32 cx = irr::core::clamp(sw * 0.5f - panX / scale, visW * 0.5f, sw - visW * 0.5f);
    const irr::f32 cy = irr::core::clamp(sh * 0.5f - panY / scale, visH * 0.5f, sh - visH * 0.5f);
    const irr::core::rect<irr::s32> src((irr::s32)(cx - visW * 0.5f), (irr::s32)(cy - visH * 0.5f), (irr::s32)(cx + visW * 0.5f), (irr::s32)(cy + visH * 0.5f));
    driver->getMaterial2D().TextureLayer[0].BilinearFilter = true;
    driver->getMaterial2D().TextureLayer[0].TrilinearFilter = still;
    driver->enableMaterial2D(true);
    driver->draw2DImage(tex, irr::core::rect<irr::s32>(0, 0, (irr::s32)W, (irr::s32)H), src, 0, 0, false);
    driver->enableMaterial2D(false);
}

void HudMenu::drawAtmosphere(irr::video::IVideoDriver* driver, irr::f32 t, irr::f32 r)
{
    const irr::s32 W = (irr::s32)laidOut.Width, H = (irr::s32)laidOut.Height;
    //Grade and gradients: the menu side and the bottom darker, for the lettering.
    driver->draw2DRectangle(irr::video::SColor(80, 3, 10, 24), irr::core::rect<irr::s32>(0, 0, W, H));
    const irr::video::SColor deep(220, 3, 10, 24), clear(0, 3, 10, 24);
    driver->draw2DRectangle(irr::core::rect<irr::s32>(0, 0, (irr::s32)(W * 0.66f), H), deep, clear, deep, clear);
    const irr::s32 topH = (irr::s32)(260 * k);
    driver->draw2DRectangle(irr::core::rect<irr::s32>(0, 0, W, topH), irr::video::SColor(205, 3, 10, 24), irr::video::SColor(205, 3, 10, 24), clear, clear);
    const irr::s32 bottomY = (irr::s32)(H * 0.55f);
    driver->draw2DRectangle(irr::core::rect<irr::s32>(0, bottomY, W, H), clear, clear, irr::video::SColor(230, 3, 10, 24), irr::video::SColor(230, 3, 10, 24));

    irr::gui::PanelBatch b;
    b.begin(driver);
    //Vignette
    const irr::core::vector2df centre(W * 0.5f, H * 0.5f);
    const irr::f32 R = std::sqrt((irr::f32)(W * W + H * H)) * 0.5f;
    b.sector(centre, R * 0.55f, R * 1.02f, 0, 360, irr::video::SColor(0, 0, 4, 12), irr::video::SColor(170, 0, 4, 12), false);
    //A faint band of light running down the screen
    const irr::f32 bandH = 140 * k;
    const irr::f32 y = std::fmod(t / 9.0f, 1.0f) * (H + 2 * bandH) - bandH;
    const irr::video::SColor glow(14, 140, 200, 255), none(0, 140, 200, 255);
    b.rectV(irr::core::rect<irr::f32>(0, y - bandH * 0.5f, (irr::f32)W, y), none, glow);
    b.rectV(irr::core::rect<irr::f32>(0, y, (irr::f32)W, y + bandH * 0.5f), glow, none);
    //Specks of light
    const irr::f32 a = easeOut(r / 2.0f);
    for (size_t i = 0; i < specks.size(); i++) {
        const Speck& s = specks[i];
        const irr::f32 px = std::fmod(s.x + s.vx * t + 10.0f, 1.0f) * W;
        const irr::f32 py = (1.0f - std::fmod(s.y + s.vy * t, 1.0f)) * H;
        const irr::f32 twinkle = 0.45f + 0.55f * (0.5f + 0.5f * std::sin(t * s.rate + s.phase));
        const irr::video::SColor c((irr::u32)(90 * twinkle * a), 190, 225, 255);
        b.disc(irr::core::vector2df(px, py), s.size * k, c, fade(c, 0.0f));
    }
    b.flush();
}

void HudMenu::drawFrame(irr::video::IVideoDriver* driver, irr::f32 r)
{
    const irr::f32 W = (irr::f32)laidOut.Width, H = (irr::f32)laidOut.Height;
    const irr::f32 p = easeOut((r - 0.1f) / 0.8f);
    if (p <= 0) { return; }
    irr::gui::PanelBatch b;
    b.begin(driver);
    //Corner brackets
    const irr::video::SColor c = fade(irr::video::SColor(120, 200, 226, 255), p);
    const irr::f32 in = 26 * k, arm = 44 * k * p, w = std::max(1.0f, 2 * k);
    b.rect(irr::core::rect<irr::f32>(in, in, in + arm, in + w), c);
    b.rect(irr::core::rect<irr::f32>(in, in, in + w, in + arm), c);
    b.rect(irr::core::rect<irr::f32>(W - in - arm, in, W - in, in + w), c);
    b.rect(irr::core::rect<irr::f32>(W - in - w, in, W - in, in + arm), c);
    b.rect(irr::core::rect<irr::f32>(in, H - in - w, in + arm, H - in), c);
    b.rect(irr::core::rect<irr::f32>(in, H - in - arm, in + w, H - in), c);
    b.rect(irr::core::rect<irr::f32>(W - in - arm, H - in - w, W - in, H - in), c);
    b.rect(irr::core::rect<irr::f32>(W - in - w, H - in - arm, W - in, H - in), c);
    //Ruler under the header, drawn out from the left
    const irr::f32 x0 = mx, x1 = mx + (W - 2 * mx) * p;
    b.rect(irr::core::rect<irr::f32>(x0, headerH, x1, headerH + std::max(1.0f, k)), fade(irr::video::SColor(70, 200, 226, 255), p));
    int n = 0;
    for (irr::f32 x = x0; x <= x1; x += 48 * k, n++) {
        const irr::f32 len = (n % 4 == 0) ? 9 * k : 4 * k;
        b.rect(irr::core::rect<irr::f32>(x, headerH, x + std::max(1.0f, k), headerH + len), fade(irr::video::SColor(60, 200, 226, 255), p));
    }
    b.rectH(irr::core::rect<irr::f32>(x0, headerH - std::max(1.0f, k), x0 + 160 * k * p, headerH + std::max(1.0f, k)), fade(cAccent, p), fade(cAccent, 0.0f));
    b.flush();
}

void HudMenu::drawHeader(irr::video::IVideoDriver* driver, irr::f32 r)
{
    const irr::f32 W = (irr::f32)laidOut.Width;
    const irr::f32 a = easeOut((r - 0.25f) / 0.6f);
    if (a <= 0) { return; }
    const irr::f32 slide = (1.0f - a) * -24 * k;
    const irr::core::vector2df badge(mx + 30 * k + slide, 78 * k);
    irr::gui::PanelBatch b;
    b.begin(driver);
    b.disc(badge, 30 * k, fade(irr::video::SColor(255, 50, 140, 240), a), fade(irr::video::SColor(255, 20, 84, 180), a));
    b.sector(badge, 30 * k, 31.5f * k, 0, 360, fade(irr::video::SColor(200, 170, 220, 255), a), fade(irr::video::SColor(200, 170, 220, 255), a));
    drawIcon(b, Icon_Helm, badge, 18 * k, fade(cWhite, a));
    const bool overMinimise = windowControls && minimiseButton.isPointInside(mouse);
    if (windowControls) {
        roundRect(b, minimiseButton, 8 * k, fade(irr::video::SColor(overMinimise ? 120 : 50, 40, 90, 160), a), fade(irr::video::SColor(overMinimise ? 120 : 50, 30, 70, 130), a));
        roundRectOutline(b, minimiseButton, 8 * k, 1.0f, fade(irr::video::SColor(overMinimise ? 220 : 110, 170, 210, 255), a));
        const irr::core::vector2df c = minimiseButton.getCenter();
        b.rect(irr::core::rect<irr::f32>(c.X - 10 * k, c.Y + 5 * k, c.X + 10 * k, c.Y + 7 * k), fade(cWhite, a));
    }
    b.flush();
    fonts.brand->draw(L"NAUTITECH", mx + 76 * k + slide, 78 * k - fonts.brand->capHeight() - 5 * k, fade(cWhite, a), 4 * k);
    fonts.sub->draw(french ? L"SIMULATEUR DE NAVIGATION MARITIME" : L"MARITIME NAVIGATION SIMULATOR", mx + 77 * k + slide, 78 * k + 9 * k,
        fade(cDim, a), 3 * k);
    //Station, right
    if (!station.empty()) {
        const irr::f32 right = windowControls ? minimiseButton.UpperLeftCorner.X - 22 * k : W - mx;
        const std::wstring label = french ? L"POSTE" : L"STATION";
        const irr::f32 nameW = fonts.small->width(station, 1.5f * k);
        fonts.small->draw(station, right - nameW, 75 * k - fonts.small->capHeight() * 0.5f, fade(cText, a), 1.5f * k);
        const irr::f32 labelW = fonts.tiny->width(label, 3 * k);
        fonts.tiny->draw(label, right - nameW - 16 * k - labelW, 75 * k - fonts.tiny->capHeight() * 0.5f, fade(cAccentSoft, a), 3 * k);
    }
}

void HudMenu::drawMenu(irr::video::IVideoDriver* driver, irr::f32 t, irr::f32 r)
{
    if (page < 0 || page >= (int)pages.size()) { return; }
    const Page& pg = pages[page];
    const int n = (int)pg.items.size();
    const bool entering = (pageAt <= revealAt + 0.001f);
    //Each row eases in: at the reveal from the left, one after the other; on a page change from the side.
    std::vector<irr::f32> shown(n), offset(n);
    for (int i = 0; i < n; i++) {
        if (entering) {
            shown[i] = easeOut((r - 0.35f - 0.06f * i) / 0.45f);
            offset[i] = (1.0f - shown[i]) * -70 * k;
        }
        else {
            shown[i] = easeOut((t - pageAt - 0.035f * i) / 0.3f);
            offset[i] = (1.0f - shown[i]) * (pageForward ? 48 : -48) * k;
        }
    }
    const irr::f32 headingAlpha = entering ? easeOut((r - 0.3f) / 0.5f) : easeOut((t - pageAt) / 0.3f);

    irr::gui::PanelBatch b;
    b.begin(driver);
    //Heading mark
    const irr::f32 hy = menuTop - 50 * k;
    b.rect(irr::core::rect<irr::f32>(menuX, hy, menuX + 4 * k, hy + 22 * k), fade(cAccent, headingAlpha));
    //Highlight of the selected row
    if (n > 0 && selected >= 0 && selected < n) {
        const irr::f32 a = shown[selected];
        const irr::f32 y = menuTop + selectionY * rowH;
        const irr::core::rect<irr::f32> hl(menuX - 24 * k + offset[selected], y + 5 * k, menuX + rowW + offset[selected], y + rowH - 5 * k);
        const irr::f32 flash = 1.0f - clamp01((t - activateAt) / 0.35f);
        const irr::video::SColor strong((irr::u32)(std::min(255.0f, 205 + 50 * flash) * a), 30 + (irr::u32)(60 * flash), 118 + (irr::u32)(60 * flash), 232);
        b.rectH(hl, strong, irr::video::SColor(0, 30, 118, 232));
        b.rectH(irr::core::rect<irr::f32>(hl.UpperLeftCorner.X, hl.UpperLeftCorner.Y, hl.LowerRightCorner.X, hl.UpperLeftCorner.Y + std::max(1.0f, k)),
            fade(irr::video::SColor(170, 160, 215, 255), a), irr::video::SColor(0, 160, 215, 255));
        b.rectH(irr::core::rect<irr::f32>(hl.UpperLeftCorner.X, hl.LowerRightCorner.Y - std::max(1.0f, k), hl.LowerRightCorner.X, hl.LowerRightCorner.Y),
            fade(irr::video::SColor(120, 160, 215, 255), a), irr::video::SColor(0, 160, 215, 255));
        b.rect(irr::core::rect<irr::f32>(hl.UpperLeftCorner.X, hl.UpperLeftCorner.Y, hl.UpperLeftCorner.X + 5 * k, hl.LowerRightCorner.Y), fade(irr::video::SColor(255, 170, 225, 255), a));
        //A sheen sweeps across when the selection changes
        const irr::f32 s = (t - selectAt) / 0.55f;
        if (s >= 0 && s <= 1) {
            const irr::f32 x = hl.UpperLeftCorner.X + s * (hl.getWidth() + 120 * k) - 60 * k, w = 46 * k, skew = 18 * k;
            const irr::video::SColor sheen((irr::u32)(70 * (1.0f - s) * a), 255, 255, 255);
            b.quad(irr::core::vector2df(x + skew, hl.UpperLeftCorner.Y), irr::core::vector2df(x + skew + w, hl.UpperLeftCorner.Y),
                irr::core::vector2df(x + w, hl.LowerRightCorner.Y), irr::core::vector2df(x, hl.LowerRightCorner.Y), sheen);
        }
    }
    //Row furniture: padlocks, chevrons, switches
    for (int i = 0; i < n; i++) {
        const HudItem& it = pg.items[i];
        const irr::core::rect<irr::f32> row = rowRect(i) + irr::core::vector2df(offset[i], 0);
        const irr::f32 cy = row.getCenter().Y;
        const bool sel = (i == selected);
        const irr::f32 a = shown[i] * (sel ? 1.0f : 0.75f);
        if (it.page >= 0) {
            const irr::f32 x = row.LowerRightCorner.X - 30 * k;
            b.line(irr::core::vector2df(x - 6 * k, cy - 9 * k), irr::core::vector2df(x + 3 * k, cy), 2.4f * k, fade(cWhite, a));
            b.line(irr::core::vector2df(x - 6 * k, cy + 9 * k), irr::core::vector2df(x + 3 * k, cy), 2.4f * k, fade(cWhite, a));
        }
        if (it.option >= 0) {
            const bool on = optionText(it.option) == (french ? L"OUI" : L"ON");
            const irr::core::rect<irr::f32> sw(row.LowerRightCorner.X - 84 * k, cy - 13 * k, row.LowerRightCorner.X - 30 * k, cy + 13 * k);
            roundRect(b, sw, 13 * k, fade(on ? irr::video::SColor(255, 46, 150, 250) : irr::video::SColor(200, 30, 44, 64), shown[i]),
                fade(on ? irr::video::SColor(255, 30, 112, 214) : irr::video::SColor(200, 22, 34, 52), shown[i]));
            roundRectOutline(b, sw, 13 * k, 1.0f, fade(irr::video::SColor(on ? 200 : 120, 170, 215, 255), shown[i]));
            b.disc(irr::core::vector2df(on ? sw.LowerRightCorner.X - 13 * k : sw.UpperLeftCorner.X + 13 * k, cy), 9.5f * k, fade(cWhite, shown[i]), fade(cWhite, shown[i]));
        }
        if (it.back) {
            drawIcon(b, Icon_Back, irr::core::vector2df(menuX + 12 * k + offset[i], cy), 12 * k, fade(cAccentSoft, shown[i]));
        }
    }
    b.flush();

    //Text
    fonts.heading->draw(hudUpper(pg.heading), menuX + 18 * k, hy + 11 * k - fonts.heading->capHeight() * 0.5f, fade(cAccentSoft, headingAlpha), 4 * k);
    irr::gui::PanelBatch locks;
    locks.begin(driver);
    for (int i = 0; i < n; i++) {
        const HudItem& it = pg.items[i];
        const bool sel = (i == selected);
        const irr::f32 glow = std::max(0.0f, 1.0f - std::fabs(selectionY - i));
        const irr::core::rect<irr::f32> row = rowRect(i) + irr::core::vector2df(offset[i], 0);
        const irr::f32 cy = row.getCenter().Y;
        if (!it.back) {
            wchar_t number[8];
            swprintf(number, 8, L"%02d", i + 1);
            fonts.index->draw(number, menuX + offset[i], cy - fonts.index->capHeight() * 0.5f,
                fade(sel ? cAccentSoft : cAccent, shown[i] * (0.55f + 0.45f * glow)), 1.5f * k);
        }
        const irr::f32 x = menuX + 60 * k + glow * 12 * k + offset[i];
        const std::wstring label = hudUpper(it.label);
        const irr::video::SColor col = sel ? cWhite : irr::video::SColor(255, 214, 226, 240);
        const irr::f32 w = fonts.menu->draw(label, x, cy - fonts.menu->capHeight() * 0.5f, fade(col, shown[i] * (0.62f + 0.38f * glow)), 1.2f * k);
        if (it.locked) {
            drawIcon(locks, Icon_Lock, irr::core::vector2df(x + w + 22 * k, cy), 10 * k, fade(sel ? cAmber : cFaint, shown[i]));
        }
        if (it.option >= 0) {
            const std::wstring value = optionText(it.option);
            const irr::f32 vw = fonts.tiny->width(value, 2 * k);
            fonts.tiny->draw(value, row.LowerRightCorner.X - 98 * k - vw, cy - fonts.tiny->capHeight() * 0.5f, fade(sel ? cWhite : cDim, shown[i]), 2 * k);
        }
    }
    locks.flush();
}

void HudMenu::drawDetails(irr::video::IVideoDriver* driver, irr::f32 t, irr::f32 r)
{
    if (panel.getWidth() <= 0 || page < 0 || page >= (int)pages.size()) { return; }
    const HudItem* it = (selected >= 0 && selected < (int)pages[page].items.size()) ? &pages[page].items[selected] : 0;
    if (!it) { return; }
    const irr::f32 base = easeOut((r - 0.65f) / 0.5f);
    const irr::f32 change = easeOut((t - selectAt) / 0.28f);
    const irr::f32 a = base * change;
    const irr::f32 slide = (1.0f - change) * 26 * k;
    const irr::core::rect<irr::s32> clip = toInt(panel);

    irr::gui::PanelBatch b;
    b.begin(driver);
    //Panel: glass, and brackets at its corners
    b.rectH(panel, fade(irr::video::SColor(190, 5, 15, 31), base), fade(irr::video::SColor(120, 5, 15, 31), base));
    const irr::video::SColor edge = fade(irr::video::SColor(150, 150, 205, 255), base);
    const irr::f32 arm = 22 * k, w = std::max(1.0f, 1.5f * k);
    const irr::f32 x0 = panel.UpperLeftCorner.X, y0 = panel.UpperLeftCorner.Y, x1 = panel.LowerRightCorner.X, y1 = panel.LowerRightCorner.Y;
    b.rect(irr::core::rect<irr::f32>(x0, y0, x0 + arm, y0 + w), edge); b.rect(irr::core::rect<irr::f32>(x0, y0, x0 + w, y0 + arm), edge);
    b.rect(irr::core::rect<irr::f32>(x1 - arm, y0, x1, y0 + w), edge); b.rect(irr::core::rect<irr::f32>(x1 - w, y0, x1, y0 + arm), edge);
    b.rect(irr::core::rect<irr::f32>(x0, y1 - w, x0 + arm, y1), edge); b.rect(irr::core::rect<irr::f32>(x0, y1 - arm, x0 + w, y1), edge);
    b.rect(irr::core::rect<irr::f32>(x1 - arm, y1 - w, x1, y1), edge); b.rect(irr::core::rect<irr::f32>(x1 - w, y1 - arm, x1, y1), edge);

    const irr::f32 left = x0 + 36 * k + slide, right = x1 - 36 * k;
    irr::f32 y = y0 + 34 * k;
    //Kind of item
    std::wstring kind;
    if (it->locked) { kind = french ? L"ADMINISTRATION" : L"ADMINISTRATION"; }
    else if (it->launches) { kind = french ? L"APPLICATION" : L"APPLICATION"; }
    else if (it->option >= 0) { kind = french ? L"OPTION DU LANCEUR" : L"LAUNCHER OPTION"; }
    else if (it->page >= 0) { kind = french ? L"MENU" : L"MENU"; }
    //Icon badge
    const irr::core::vector2df icon(left + 36 * k, y + 22 * k + 36 * k);
    b.disc(icon, 36 * k, fade(irr::video::SColor(110, 40, 120, 220), a), fade(irr::video::SColor(60, 30, 90, 180), a));
    b.sector(icon, 36 * k, 37.5f * k, 0, 360, fade(cAccent, a), fade(cAccent, a));
    drawIcon(b, it->icon, icon, 20 * k, fade(cWhite, a));
    b.flush();

    if (!kind.empty()) { fonts.tiny->draw(kind, left, y, fade(cAccentSoft, a), 3.5f * k, &clip); }
    y += 22 * k;
    //Title, beside the badge
    const irr::f32 titleX = left + 96 * k;
    const std::vector<std::wstring> titleLines = fonts.title->wrap(hudUpper(it->title.empty() ? it->label : it->title), right - titleX, 1 * k);
    const irr::f32 titleStep = fonts.title->capHeight() * 1.32f;
    irr::f32 ty = icon.Y - (std::min<size_t>(titleLines.size(), 2) * titleStep - (titleStep - fonts.title->capHeight())) * 0.5f;
    ty = std::max(ty, y + 6 * k); //under the kind of item
    for (size_t i = 0; i < titleLines.size() && i < 2; i++) {
        fonts.title->draw(titleLines[i], titleX, ty, fade(cWhite, a), 1 * k, &clip);
        ty += titleStep;
    }
    y = std::max(icon.Y + 36 * k, ty - titleStep + fonts.title->capHeight()) + 28 * k;

    b.begin(driver);
    b.rectH(irr::core::rect<irr::f32>(left, y, left + 90 * k, y + 3 * k), fade(cAccent, a), fade(cAccent, 0.0f));
    b.flush();
    y += 30 * k;
    //Description
    const std::vector<std::wstring> lines = fonts.text->wrap(it->text, right - left);
    const irr::f32 lineStep = fonts.text->capHeight() * 1.95f;
    for (size_t i = 0; i < lines.size() && i < 5; i++) {
        fonts.text->draw(lines[i], left, y, fade(irr::video::SColor(255, 206, 220, 236), a), 0, &clip);
        y += lineStep;
    }
    y += 8 * k;
    //Tags
    if (!it->tags.empty()) {
        irr::f32 x = left;
        const irr::f32 h = 30 * k;
        std::vector<std::pair<irr::f32, irr::f32> > places;
        b.begin(driver);
        for (size_t i = 0; i < it->tags.size(); i++) {
            const irr::f32 w2 = fonts.tiny->width(it->tags[i], 2 * k) + 28 * k;
            if (x + w2 > right && x > left) { x = left; y += h + 10 * k; }
            const irr::core::rect<irr::f32> tag(x, y, x + w2, y + h);
            roundRect(b, tag, 4 * k, fade(irr::video::SColor(60, 64, 160, 255), a), fade(irr::video::SColor(40, 64, 160, 255), a));
            roundRectOutline(b, tag, 4 * k, 1.0f, fade(irr::video::SColor(150, 120, 190, 255), a));
            places.push_back(std::make_pair(x + 14 * k, y + h * 0.5f));
            x += w2 + 10 * k;
        }
        b.flush();
        for (size_t i = 0; i < it->tags.size(); i++) {
            fonts.tiny->draw(it->tags[i], places[i].first, places[i].second - fonts.tiny->capHeight() * 0.5f, fade(cText, a), 2 * k, &clip);
        }
        y += h + 22 * k;
    }
    //Facts
    for (size_t i = 0; i < it->facts.size(); i++) {
        if (y + 30 * k > y1 - 70 * k) { break; }
        const irr::f32 lw = fonts.tiny->width(it->facts[i].first, 2.5f * k);
        fonts.tiny->draw(it->facts[i].first, left, y + 4 * k, fade(cFaint, a), 2.5f * k, &clip);
        fonts.small->draw(it->facts[i].second, left + std::max(lw + 16 * k, 190 * k), y + 4 * k + fonts.tiny->capHeight() - fonts.small->capHeight(),
            fade(cText, a), 0.5f * k, &clip);
        y += 36 * k;
    }
    //What Enter does
    const irr::f32 py = y1 - 40 * k;
    irr::f32 px = drawKeyHint(driver, french ? L"ENTR\u00C9E" : L"ENTER", it->action.empty() ? (french ? L"S\u00C9LECTIONNER" : L"SELECT") : it->action,
        left, py, a);
    if (it->locked) {
        fonts.tiny->draw(french ? L"MOT DE PASSE ADMINISTRATEUR" : L"ADMINISTRATOR PASSWORD", px + 24 * k, py - fonts.tiny->capHeight() * 0.5f,
            fade(cAmber, a), 2.5f * k, &clip);
    }
}

irr::f32 HudMenu::drawKeyHint(irr::video::IVideoDriver* driver, const std::wstring& key, const std::wstring& text, irr::f32 x, irr::f32 cy, irr::f32 alpha)
{
    const bool arrows = (key == L"UPDOWN");
    const irr::f32 h = 30 * k;
    const irr::f32 keyW = arrows ? 48 * k : std::max(h, fonts.tiny->width(key, 1.5f * k) + 18 * k);
    const irr::core::rect<irr::f32> cap(x, cy - h * 0.5f, x + keyW, cy + h * 0.5f);
    irr::gui::PanelBatch b;
    b.begin(driver);
    roundRect(b, cap, 5 * k, fade(irr::video::SColor(70, 255, 255, 255), alpha), fade(irr::video::SColor(40, 255, 255, 255), alpha));
    roundRectOutline(b, cap, 5 * k, 1.0f, fade(irr::video::SColor(170, 230, 240, 255), alpha));
    if (arrows) {
        const irr::f32 c1 = cap.UpperLeftCorner.X + keyW * 0.32f, c2 = cap.UpperLeftCorner.X + keyW * 0.68f, s = 6 * k;
        b.tri(irr::core::vector2df(c1, cy - s), irr::core::vector2df(c1 + s, cy + s * 0.7f), irr::core::vector2df(c1 - s, cy + s * 0.7f), fade(cWhite, alpha));
        b.tri(irr::core::vector2df(c2, cy + s), irr::core::vector2df(c2 - s, cy - s * 0.7f), irr::core::vector2df(c2 + s, cy - s * 0.7f), fade(cWhite, alpha));
    }
    b.flush();
    if (!arrows) { fonts.tiny->drawIn(key, cap, fade(cWhite, alpha), HudFont::Centre, 1.5f * k); }
    const irr::f32 tw = fonts.tiny->draw(text, cap.LowerRightCorner.X + 12 * k, cy - fonts.tiny->capHeight() * 0.5f, fade(cDim, alpha), 2.5f * k);
    return cap.LowerRightCorner.X + 12 * k + tw;
}

void HudMenu::drawRadar(irr::video::IVideoDriver* driver, irr::f32 t, irr::f32 r)
{
    if (radarRadius <= 0) { return; }
    const irr::f32 a = easeOut((r - 0.8f) / 0.7f);
    if (a <= 0) { return; }
    const irr::core::vector2df c = radarCentre;
    const irr::f32 R = radarRadius;
    irr::gui::PanelBatch b;
    b.begin(driver);
    b.disc(c, R, fade(irr::video::SColor(170, 4, 22, 40), a), fade(irr::video::SColor(150, 4, 16, 30), a));
    for (int i = 1; i <= 2; i++) {
        b.sector(c, R * i / 3.0f - 0.5f, R * i / 3.0f + 0.5f, 0, 360, fade(irr::video::SColor(60, 120, 200, 255), a), fade(irr::video::SColor(60, 120, 200, 255), a));
    }
    b.sector(c, R - 1.0f, R + 1.0f, 0, 360, fade(irr::video::SColor(170, 120, 200, 255), a), fade(irr::video::SColor(170, 120, 200, 255), a));
    b.line(irr::core::vector2df(c.X - R, c.Y), irr::core::vector2df(c.X + R, c.Y), 1.0f, fade(irr::video::SColor(45, 120, 200, 255), a));
    b.line(irr::core::vector2df(c.X, c.Y - R), irr::core::vector2df(c.X, c.Y + R), 1.0f, fade(irr::video::SColor(45, 120, 200, 255), a));
    for (int d = 0; d < 360; d += 10) {
        const irr::f32 len = (d % 30 == 0) ? 9 * k : 4 * k;
        b.line(irr::gui::panelPolar(c, R + 3 * k, (irr::f32)d), irr::gui::panelPolar(c, R + 3 * k + len, (irr::f32)d), 1.0f, fade(irr::video::SColor(120, 160, 210, 255), a));
    }
    //Sweep and its fading trail
    const irr::f32 sweep = std::fmod(t * 50.0f, 360.0f);
    for (int j = 0; j < 24; j++) {
        const irr::f32 q = 1.0f - j / 24.0f;
        const irr::video::SColor col((irr::u32)(85 * q * q * a), 70, 190, 255);
        b.sector(c, 0, R, sweep - (j + 1) * 2.5f, sweep - j * 2.5f, col, col, false);
    }
    b.line(c, irr::gui::panelPolar(c, R, sweep), 2.0f * k, fade(irr::video::SColor(230, 150, 225, 255), a));
    //Echoes, bright as the sweep passes and fading after
    static const irr::f32 echoes[][3] = { { 32, 0.62f, 0.0f }, { 75, 0.35f, 0.4f }, { 118, 0.82f, -0.3f }, { 160, 0.5f, 0.2f }, { 205, 0.72f, 0.5f },
        { 248, 0.28f, -0.2f }, { 290, 0.88f, 0.3f }, { 322, 0.46f, -0.4f }, { 350, 0.7f, 0.1f } };
    for (size_t i = 0; i < sizeof(echoes) / sizeof(echoes[0]); i++) {
        const irr::f32 angle = echoes[i][0] + std::sin(t * 0.05f + i) * 4.0f;
        const irr::f32 dist = echoes[i][1] + echoes[i][2] * 0.04f * std::sin(t * 0.1f + i);
        const irr::f32 since = std::fmod(sweep - angle + 720.0f, 360.0f);
        const irr::f32 glow = std::pow(clamp01(1.0f - since / 330.0f), 2.0f);
        const irr::video::SColor col((irr::u32)(255 * glow * a), 160, 240, 255);
        b.disc(irr::gui::panelPolar(c, R * dist, angle), 3.2f * k, col, fade(col, 0.2f));
    }
    //Own ship
    b.tri(irr::core::vector2df(c.X, c.Y - 7 * k), irr::core::vector2df(c.X + 4.5f * k, c.Y + 5 * k), irr::core::vector2df(c.X - 4.5f * k, c.Y + 5 * k), fade(cWhite, a));
    b.flush();
    const std::wstring label = french ? L"RADAR  \u00B7  6 NM" : L"RADAR  \u00B7  6 NM";
    fonts.tiny->drawIn(label, irr::core::rect<irr::f32>(c.X - R, c.Y + R + 14 * k, c.X + R, c.Y + R + 34 * k), fade(cDim, a), HudFont::Centre, 3 * k);
}

void HudMenu::drawFooter(irr::video::IVideoDriver* driver, irr::f32 r)
{
    const irr::f32 W = (irr::f32)laidOut.Width, H = (irr::f32)laidOut.Height;
    const irr::f32 a = easeOut((r - 0.9f) / 0.5f);
    if (a <= 0) { return; }
    irr::gui::PanelBatch b;
    b.begin(driver);
    b.rectH(irr::core::rect<irr::f32>(mx, footerY, W - mx, footerY + std::max(1.0f, k)), fade(irr::video::SColor(90, 200, 226, 255), a),
        fade(irr::video::SColor(20, 200, 226, 255), a));
    b.flush();
    const irr::f32 cy = (footerY + H) * 0.5f;
    irr::f32 x = mx;
    x = drawKeyHint(driver, L"UPDOWN", french ? L"NAVIGUER" : L"NAVIGATE", x, cy, a) + 34 * k;
    x = drawKeyHint(driver, french ? L"ENTR\u00C9E" : L"ENTER", french ? L"S\u00C9LECTIONNER" : L"SELECT", x, cy, a) + 34 * k;
    x = drawKeyHint(driver, french ? L"\u00C9CHAP" : L"ESC", page != 0 ? (french ? L"RETOUR" : L"BACK") : (french ? L"QUITTER" : L"QUIT"), x, cy, a) + 34 * k;
    if (W > 1500 * k) { drawKeyHint(driver, L"F11", french ? L"PLEIN \u00C9CRAN" : L"FULL SCREEN", x, cy, a); }
    if (!version.empty()) {
        const irr::f32 vw = fonts.tiny->width(version, 2.5f * k);
        fonts.tiny->draw(version, W - mx - vw, cy - fonts.tiny->capHeight() * 0.5f, fade(cFaint, a), 2.5f * k);
    }
}

void HudMenu::drawToast(irr::video::IVideoDriver* driver, irr::f32 t)
{
    if (toastText.empty()) { return; }
    const irr::f32 age = t - toastAt;
    if (age > 3.2f) {
        toastText.clear();
        return;
    }
    const irr::f32 a = age < 0.25f ? age / 0.25f : (age > 2.7f ? (3.2f - age) / 0.5f : 1.0f);
    const irr::f32 W = (irr::f32)laidOut.Width;
    const std::wstring name = hudUpper(toastText);
    const irr::f32 w = std::max(fonts.small->width(name, 2 * k) + 110 * k, 440 * k), h = 78 * k;
    const irr::f32 x = (W - w) * 0.5f, y = footerY - 40 * k - h;
    const irr::core::rect<irr::f32> box(x, y, x + w, y + h);
    irr::gui::PanelBatch b;
    b.begin(driver);
    b.rect(box, fade(irr::video::SColor(225, 5, 16, 32), a));
    b.rect(irr::core::rect<irr::f32>(x, y, x + 5 * k, y + h), fade(cAccent, a));
    b.rect(irr::core::rect<irr::f32>(x, y + h - 3 * k, x + w * easeOut(age / 2.6f), y + h), fade(cAccent, a));
    //Busy marks
    for (int i = 0; i < 3; i++) {
        const irr::f32 p = 0.5f + 0.5f * std::sin(t * 9.0f - i * 0.9f);
        b.rect(irr::core::rect<irr::f32>(x + w - 64 * k + i * 14 * k, y + h * 0.5f - 4 * k, x + w - 56 * k + i * 14 * k, y + h * 0.5f + 4 * k),
            fade(irr::video::SColor(255, 150, 215, 255), a * (0.3f + 0.7f * p)));
    }
    b.flush();
    fonts.tiny->draw(french ? L"LANCEMENT" : L"STARTING", x + 28 * k, y + 16 * k, fade(cAccentSoft, a), 3.5f * k);
    fonts.small->draw(name, x + 28 * k, y + h - 22 * k - fonts.small->capHeight(), fade(cWhite, a), 2 * k);
}

void HudMenu::drawQuitPrompt(irr::video::IVideoDriver* driver, irr::f32 t)
{
    const irr::f32 W = (irr::f32)laidOut.Width, H = (irr::f32)laidOut.Height;
    const irr::f32 a = easeOut((t - quitAt) / 0.22f);
    driver->draw2DRectangle(irr::video::SColor((irr::u32)(170 * a), 0, 4, 12), irr::core::rect<irr::s32>(0, 0, (irr::s32)W, (irr::s32)H));
    irr::gui::PanelBatch b;
    b.begin(driver);
    const irr::f32 grow = (1.0f - a) * 12 * k;
    irr::core::rect<irr::f32> p = quitPanel;
    p.UpperLeftCorner += irr::core::vector2df(grow, grow);
    p.LowerRightCorner -= irr::core::vector2df(grow, grow);
    b.rectV(p, fade(irr::video::SColor(245, 12, 30, 56), a), fade(irr::video::SColor(245, 6, 16, 32), a));
    b.rect(irr::core::rect<irr::f32>(p.UpperLeftCorner.X, p.UpperLeftCorner.Y, p.LowerRightCorner.X, p.UpperLeftCorner.Y + 3 * k), fade(cAccent, a));
    roundRectOutline(b, p, 0.1f, 1.0f, fade(irr::video::SColor(120, 150, 205, 255), a));
    const wchar_t* labels[2] = { french ? L"QUITTER" : L"QUIT", french ? L"ANNULER" : L"CANCEL" };
    for (int i = 0; i < 2; i++) {
        const bool on = (quitChoice == i);
        const irr::video::SColor fill = (i == 0) ? cDanger : irr::video::SColor(255, 40, 120, 220);
        if (on) { b.rect(quitButtons[i], fade(fill, a)); }
        else { b.rect(quitButtons[i], fade(irr::video::SColor(60, 255, 255, 255), a)); }
        roundRectOutline(b, quitButtons[i], 0.1f, 1.0f, fade(irr::video::SColor(on ? 255 : 120, 255, 255, 255), a));
    }
    b.flush();
    fonts.quit->drawIn(french ? L"QUITTER NAUTITECH ?" : L"QUIT NAUTITECH?", irr::core::rect<irr::f32>(p.UpperLeftCorner.X, p.UpperLeftCorner.Y + 40 * k,
        p.LowerRightCorner.X, p.UpperLeftCorner.Y + 100 * k), fade(cWhite, a), HudFont::Centre, 2 * k);
    fonts.text->drawIn(french ? L"Le lanceur va se fermer." : L"The launcher will close.", irr::core::rect<irr::f32>(p.UpperLeftCorner.X, p.UpperLeftCorner.Y + 108 * k,
        p.LowerRightCorner.X, p.UpperLeftCorner.Y + 148 * k), fade(cDim, a), HudFont::Centre);
    for (int i = 0; i < 2; i++) {
        fonts.small->drawIn(labels[i], quitButtons[i], fade(cWhite, a), HudFont::Centre, 3 * k);
    }
}
