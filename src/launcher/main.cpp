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

#include "../ScreenChooser.hpp" //screens of the desk, and which already show the simulator

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
//Launcher look: the background picture full-window, a dark gradient at the bottom, and custom-drawn
//cards (main applications) and chips (settings, shortcuts, quit) on it. Everything is drawn with
//Irrlicht 2D triangles (GUIPanelDraw.hpp), so edges are smooth and nothing depends on image files
//apart from the background. Cards and chips are GUI elements that send EGET_BUTTON_CLICKED like
//ordinary buttons, so the Receiver above launches exactly as before.
//=================================================================================================

namespace Theme {
    const irr::video::SColor text(255, 244, 247, 251);
    const irr::video::SColor textDim(255, 178, 192, 208);
    const irr::video::SColor accent(255, 64, 156, 240);
    const irr::video::SColor accentHi(255, 120, 190, 255);
    const irr::video::SColor primaryTop(255, 36, 122, 222);
    const irr::video::SColor primaryBottom(255, 16, 78, 168);
    const irr::video::SColor glass(158, 9, 20, 36);
    const irr::video::SColor glassHover(190, 16, 34, 58);
    const irr::video::SColor border(64, 255, 255, 255);
    const irr::video::SColor danger(255, 214, 72, 72);
    const irr::video::SColor shade(255, 3, 10, 22);   //bottom gradient
}

irr::video::SColor mixColour(irr::video::SColor a, irr::video::SColor b, irr::f32 t)
{
    return b.getInterpolated(a, irr::core::clamp(t, 0.0f, 1.0f));
}

//Rounded rectangle, vertical gradient, soft edges.
void roundRect(irr::gui::PanelBatch& b, const irr::core::rect<irr::f32>& r, irr::f32 rad, irr::video::SColor top, irr::video::SColor bottom)
{
    using irr::core::vector2df;
    const irr::f32 x0 = r.UpperLeftCorner.X, y0 = r.UpperLeftCorner.Y, x1 = r.LowerRightCorner.X, y1 = r.LowerRightCorner.Y;
    rad = irr::core::min_(rad, irr::core::min_((x1 - x0) * 0.5f, (y1 - y0) * 0.5f));
    const irr::f32 h = y1 - y0;
    const irr::video::SColor cTop = mixColour(top, bottom, rad / h);
    const irr::video::SColor cBot = mixColour(top, bottom, 1.0f - rad / h);
    b.rectV(irr::core::rect<irr::f32>(x0 + rad, y0, x1 - rad, y0 + rad), top, cTop);
    b.rectV(irr::core::rect<irr::f32>(x0, y0 + rad, x1, y1 - rad), cTop, cBot);
    b.rectV(irr::core::rect<irr::f32>(x0 + rad, y1 - rad, x1 - rad, y1), cBot, bottom);
    b.sector(vector2df(x0 + rad, y0 + rad), 0, rad, 270, 360, cTop, top);
    b.sector(vector2df(x1 - rad, y0 + rad), 0, rad, 0, 90, cTop, top);
    b.sector(vector2df(x1 - rad, y1 - rad), 0, rad, 90, 180, cBot, bottom);
    b.sector(vector2df(x0 + rad, y1 - rad), 0, rad, 180, 270, cBot, bottom);
}

void roundRectOutline(irr::gui::PanelBatch& b, const irr::core::rect<irr::f32>& r, irr::f32 rad, irr::f32 w, irr::video::SColor col)
{
    using irr::core::vector2df;
    const irr::f32 x0 = r.UpperLeftCorner.X, y0 = r.UpperLeftCorner.Y, x1 = r.LowerRightCorner.X, y1 = r.LowerRightCorner.Y;
    rad = irr::core::min_(rad, irr::core::min_((x1 - x0) * 0.5f, (y1 - y0) * 0.5f));
    b.rect(irr::core::rect<irr::f32>(x0 + rad, y0, x1 - rad, y0 + w), col);
    b.rect(irr::core::rect<irr::f32>(x0 + rad, y1 - w, x1 - rad, y1), col);
    b.rect(irr::core::rect<irr::f32>(x0, y0 + rad, x0 + w, y1 - rad), col);
    b.rect(irr::core::rect<irr::f32>(x1 - w, y0 + rad, x1, y1 - rad), col);
    b.sector(vector2df(x0 + rad, y0 + rad), rad - w, rad, 270, 360, col, col);
    b.sector(vector2df(x1 - rad, y0 + rad), rad - w, rad, 0, 90, col, col);
    b.sector(vector2df(x1 - rad, y1 - rad), rad - w, rad, 90, 180, col, col);
    b.sector(vector2df(x0 + rad, y1 - rad), rad - w, rad, 180, 270, col, col);
}

enum LauncherIcon { Icon_Helm, Icon_Route, Icon_Flame, Icon_Network, Icon_Gear, Icon_Keys, Icon_Power, Icon_Compass };

//Line icons, drawn in a box of half-size s around c.
void drawIcon(irr::gui::PanelBatch& b, LauncherIcon icon, irr::core::vector2df c, irr::f32 s, irr::video::SColor col)
{
    using irr::core::vector2df;
    using irr::gui::panelPolar;
    const irr::f32 lw = irr::core::max_(1.6f, s * 0.13f);
    switch (icon) {
    case Icon_Helm: {   //ship's wheel
        b.sector(c, s * 0.50f, s * 0.50f + lw, 0, 360, col, col);
        b.disc(c, s * 0.20f, col, col);
        for (int i = 0; i < 8; i++) {
            const irr::f32 a = 22.5f + 45.0f * i;
            b.line(panelPolar(c, s * 0.18f, a), panelPolar(c, s * 0.86f, a), lw, col);
            b.disc(panelPolar(c, s * 0.92f, a), lw * 0.85f, col, col);
        }
        break;
    }
    case Icon_Route: {  //planned track with waypoints
        const vector2df p[4] = { vector2df(c.X - s * 0.80f, c.Y + s * 0.62f), vector2df(c.X - s * 0.20f, c.Y + s * 0.05f),
            vector2df(c.X + s * 0.28f, c.Y + s * 0.38f), vector2df(c.X + s * 0.78f, c.Y - s * 0.55f) };
        for (int i = 0; i < 3; i++) { b.line(p[i], p[i + 1], lw, col); }
        for (int i = 0; i < 3; i++) { b.disc(p[i], lw * 1.25f, col, col); }
        b.sector(p[3], s * 0.18f, s * 0.18f + lw, 0, 360, col, col);
        break;
    }
    case Icon_Flame: {  //flame: outer tongues in the icon colour, a warm core inside
        static const irr::f32 outer[][2] = {
            { 0.00f, 1.00f }, { 0.42f, 0.86f }, { 0.66f, 0.52f }, { 0.64f, 0.10f }, { 0.50f, -0.24f }, { 0.36f, -0.02f },
            { 0.30f, -0.48f }, { 0.12f, -0.78f }, { -0.02f, -1.05f }, { -0.18f, -0.62f }, { -0.42f, -0.36f },
            { -0.56f, -0.58f }, { -0.64f, -0.12f }, { -0.66f, 0.40f }, { -0.44f, 0.84f } };
        static const irr::f32 inner[][2] = {
            { 0.00f, 0.98f }, { 0.30f, 0.84f }, { 0.38f, 0.50f }, { 0.24f, 0.10f }, { 0.04f, -0.30f },
            { -0.10f, 0.02f }, { -0.30f, 0.30f }, { -0.34f, 0.70f } };
        const int no = sizeof(outer) / sizeof(outer[0]), ni = sizeof(inner) / sizeof(inner[0]);
        const irr::f32 k = s * 0.90f;
        const vector2df co(c.X, c.Y + 0.30f * k), ci(c.X, c.Y + 0.55f * k);
        for (int i = 0; i < no; i++) {
            const int n = (i + 1) % no;
            b.tri(co, vector2df(c.X + outer[i][0] * k, c.Y + outer[i][1] * k), vector2df(c.X + outer[n][0] * k, c.Y + outer[n][1] * k), col);
        }
        const irr::video::SColor coreCol(col.getAlpha(), 255, 206, 96);
        for (int i = 0; i < ni; i++) {
            const int n = (i + 1) % ni;
            b.tri(ci, vector2df(c.X + inner[i][0] * k, c.Y + inner[i][1] * k), vector2df(c.X + inner[n][0] * k, c.Y + inner[n][1] * k), coreCol);
        }
        break;
    }
    case Icon_Network: {  //stations linked to a hub
        const vector2df hub(c.X, c.Y + s * 0.05f);
        const vector2df n[3] = { panelPolar(hub, s * 0.78f, 0), panelPolar(hub, s * 0.78f, 120), panelPolar(hub, s * 0.78f, 240) };
        for (int i = 0; i < 3; i++) { b.line(hub, n[i], lw, col); }
        b.disc(hub, s * 0.24f, col, col);
        for (int i = 0; i < 3; i++) { b.sector(n[i], s * 0.17f, s * 0.17f + lw, 0, 360, col, col); }
        break;
    }
    case Icon_Gear: {
        for (int i = 0; i < 8; i++) {
            const irr::f32 a = 45.0f * i;
            b.line(panelPolar(c, s * 0.50f, a), panelPolar(c, s * 0.90f, a), s * 0.30f, col);
        }
        b.sector(c, s * 0.28f, s * 0.66f, 0, 360, col, col);
        break;
    }
    case Icon_Keys: {   //keyboard
        const irr::core::rect<irr::f32> r(c.X - s * 0.95f, c.Y - s * 0.58f, c.X + s * 0.95f, c.Y + s * 0.58f);
        roundRectOutline(b, r, s * 0.18f, lw, col);
        for (int row = 0; row < 2; row++) {
            for (int k = 0; k < 4; k++) {
                const irr::f32 x = r.UpperLeftCorner.X + s * (0.38f + 0.40f * k), y = r.UpperLeftCorner.Y + s * (0.38f + 0.36f * row);
                b.rect(irr::core::rect<irr::f32>(x - s * 0.11f, y - s * 0.09f, x + s * 0.11f, y + s * 0.09f), col);
            }
        }
        b.rect(irr::core::rect<irr::f32>(c.X - s * 0.45f, c.Y + s * 0.26f, c.X + s * 0.45f, c.Y + s * 0.40f), col);
        break;
    }
    case Icon_Power: {
        b.sector(c, s * 0.62f, s * 0.62f + lw, 35, 325, col, col);
        b.line(vector2df(c.X, c.Y - s * 0.90f), vector2df(c.X, c.Y - s * 0.15f), lw, col);
        break;
    }
    case Icon_Compass: {  //compass card: ring, cardinal marks, needle (north half solid)
        b.sector(c, s * 0.86f, s * 0.86f + lw, 0, 360, col, col);
        for (int i = 0; i < 4; i++) {
            b.line(panelPolar(c, s * 0.60f, 90.0f * i), panelPolar(c, s * 0.80f, 90.0f * i), lw, col);
        }
        const vector2df north = panelPolar(c, s * 0.56f, 0), south = panelPolar(c, s * 0.56f, 180);
        const vector2df west = panelPolar(c, s * 0.19f, 270), east = panelPolar(c, s * 0.19f, 90);
        b.tri(north, east, west, col);
        b.tri(south, west, east, irr::video::SColor(col.getAlpha() / 2, col.getRed(), col.getGreen(), col.getBlue()));
        break;
    }
    }
}

//Word-wrap text into the given width.
std::vector<std::wstring> wrapText(irr::gui::IGUIFont* font, const std::wstring& text, irr::s32 width)
{
    std::vector<std::wstring> lines;
    if (!font) { lines.push_back(text); return lines; }
    std::wstring line, word;
    for (size_t i = 0; i <= text.size(); i++) {
        const wchar_t ch = (i < text.size()) ? text[i] : L' ';
        if (ch == L' ' || ch == L'\n') {
            const std::wstring candidate = line.empty() ? word : line + L" " + word;
            if (!line.empty() && (irr::s32)font->getDimension(candidate.c_str()).Width > width) {
                lines.push_back(line);
                line = word;
            }
            else {
                line = candidate;
            }
            word.clear();
            if (ch == L'\n') { lines.push_back(line); line.clear(); }
        }
        else {
            word += ch;
        }
    }
    if (!line.empty()) { lines.push_back(line); }
    return lines;
}

enum TileStyle { Tile_Primary, Tile_Card, Tile_Chip, Tile_ChipDanger };

class LauncherTile : public irr::gui::IGUIElement
{
public:
    LauncherTile(irr::gui::IGUIEnvironment* env, irr::s32 id, const std::wstring& title, const std::wstring& subtitle,
        LauncherIcon icon, TileStyle style)
        : irr::gui::IGUIElement(irr::gui::EGUIET_BUTTON, env, env->getRootGUIElement(), id, irr::core::rect<irr::s32>(0, 0, 10, 10)),
        title(title), subtitle(subtitle), icon(icon), style(style), hovered(false), pressed(false), locked(false), hover(0), lastMs(0),
        titleFont(0), subFont(0)
    {
        setTabStop(true);
        setText(title.c_str());
    }

    void setFonts(irr::gui::IGUIFont* t, irr::gui::IGUIFont* s) { titleFont = t; subFont = s; }
    //Padlock at the end of a chip: the tool it opens asks for the administrator password.
    void setLocked(bool on) { locked = on; }

    //Height a card needs at this width: icon badge, title and description (up to three lines), as draw() lays them out.
    irr::s32 neededCardHeight(irr::s32 width) const
    {
        irr::s32 h = 22 + 46 + 14;
        if (titleFont) { h += (irr::s32)titleFont->getDimension(title.c_str()).Height + 4; }
        if (subFont) {
            const size_t lines = irr::core::min_(wrapText(subFont, subtitle, width - 44).size(), (size_t)3);
            h += (irr::s32)lines * ((irr::s32)subFont->getDimension(L"Ag").Height + 1);
        }
        return h + 10;
    }

    //Width a chip needs for its label.
    irr::s32 preferredChipWidth() const
    {
        const irr::s32 tw = titleFont ? (irr::s32)titleFont->getDimension(title.c_str()).Width : 100;
        return tw + 62 + (locked ? 20 : 0);
    }

    bool isAnimating() const { return (hovered && hover < 1.0f) || (!hovered && hover > 0.0f); }

    virtual bool OnEvent(const irr::SEvent& event)
    {
        if (!isEnabled()) { return IGUIElement::OnEvent(event); }
        if (event.EventType == irr::EET_GUI_EVENT && event.GUIEvent.Caller == this) {
            if (event.GUIEvent.EventType == irr::gui::EGET_ELEMENT_HOVERED) { hovered = true; }
            if (event.GUIEvent.EventType == irr::gui::EGET_ELEMENT_LEFT) { hovered = false; pressed = false; }
        }
        if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
            if (event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN) {
                pressed = true;
                Environment->setFocus(this);
                return true;
            }
            if (event.MouseInput.Event == irr::EMIE_LMOUSE_LEFT_UP) {
                const bool click = pressed && AbsoluteRect.isPointInside(irr::core::position2di(event.MouseInput.X, event.MouseInput.Y));
                pressed = false;
                if (click) { fire(); }
                return true;
            }
        }
        if (event.EventType == irr::EET_KEY_INPUT_EVENT && !event.KeyInput.PressedDown &&
            (event.KeyInput.Key == irr::KEY_RETURN || event.KeyInput.Key == irr::KEY_SPACE)) {
            fire();
            return true;
        }
        return IGUIElement::OnEvent(event);
    }

    virtual void draw()
    {
        if (!IsVisible) { return; }
        irr::video::IVideoDriver* driver = Environment->getVideoDriver();

        //Hover eases in and out over ~150 ms.
        const irr::u32 now = g_device ? g_device->getTimer()->getRealTime() : 0;
        const irr::f32 dt = lastMs ? (irr::f32)(now - lastMs) / 1000.0f : 0.0f;
        lastMs = now;
        hover = irr::core::clamp(hover + (hovered ? 1.0f : -1.0f) * dt / 0.15f, 0.0f, 1.0f);
        const irr::f32 k = hover * hover * (3.0f - 2.0f * hover);
        const bool chip = (style == Tile_Chip || style == Tile_ChipDanger);
        const bool focusRing = Environment->hasFocus(this) && !pressed;

        irr::core::rect<irr::f32> r((irr::f32)AbsoluteRect.UpperLeftCorner.X, (irr::f32)AbsoluteRect.UpperLeftCorner.Y,
            (irr::f32)AbsoluteRect.LowerRightCorner.X, (irr::f32)AbsoluteRect.LowerRightCorner.Y);
        const irr::f32 lift = chip ? 0.0f : 3.0f * k - (pressed ? 1.0f : 0.0f);
        r.UpperLeftCorner.Y -= lift;
        r.LowerRightCorner.Y -= lift;
        const irr::f32 rad = chip ? r.getHeight() * 0.5f : 14.0f;

        irr::gui::PanelBatch b;
        b.begin(driver);
        //Shadow
        if (!chip) {
            irr::core::rect<irr::f32> s = r;
            s.UpperLeftCorner.Y += 6 + 3 * k; s.LowerRightCorner.Y += 6 + 3 * k;
            s.UpperLeftCorner.X += 2; s.LowerRightCorner.X -= 2;
            roundRect(b, s, rad, irr::video::SColor((irr::u32)(70 + 50 * k), 0, 0, 0), irr::video::SColor((irr::u32)(70 + 50 * k), 0, 0, 0));
        }
        //Body
        if (style == Tile_Primary) {
            const irr::video::SColor top = mixColour(Theme::primaryTop, irr::video::SColor(255, 66, 150, 240), k);
            const irr::video::SColor bottom = mixColour(Theme::primaryBottom, irr::video::SColor(255, 26, 98, 196), k);
            roundRect(b, r, rad, pressed ? bottom : top, bottom);
            roundRectOutline(b, r, rad, 1.0f, irr::video::SColor((irr::u32)(70 + 80 * k), 190, 225, 255));
        }
        else {
            const irr::video::SColor fill = mixColour(Theme::glass, Theme::glassHover, pressed ? 1.0f : k);
            roundRect(b, r, rad, irr::video::SColor(fill.getAlpha(), fill.getRed() + 8, fill.getGreen() + 10, fill.getBlue() + 14), fill);
            const irr::video::SColor edge = (style == Tile_ChipDanger) ? Theme::danger : Theme::accent;
            roundRectOutline(b, r, rad, 1.0f, mixColour(Theme::border, irr::video::SColor(220, edge.getRed(), edge.getGreen(), edge.getBlue()), k));
        }
        if (focusRing) {
            irr::core::rect<irr::f32> f = r;
            f.UpperLeftCorner -= irr::core::vector2df(3, 3); f.LowerRightCorner += irr::core::vector2df(3, 3);
            roundRectOutline(b, f, rad + 3, 1.5f, irr::video::SColor(150, 150, 205, 255));
        }

        //Icon
        irr::video::SColor iconCol = Theme::text;
        if (style == Tile_Card) { iconCol = mixColour(Theme::accentHi, Theme::text, k); }
        if (style == Tile_ChipDanger) { iconCol = mixColour(Theme::textDim, Theme::danger, k); }
        if (style == Tile_Chip) { iconCol = mixColour(Theme::textDim, Theme::accentHi, k); }
        irr::core::vector2df iconCentre;
        if (chip) {
            iconCentre = irr::core::vector2df(r.UpperLeftCorner.X + 24, r.getCenter().Y);
            drawIcon(b, icon, iconCentre, 9.0f, iconCol);
            if (locked) {
                //Padlock: body and shackle.
                const irr::core::vector2df c(r.LowerRightCorner.X - 26, r.getCenter().Y);
                const irr::video::SColor lockCol = mixColour(irr::video::SColor(200, 150, 168, 190), irr::video::SColor(255, 255, 200, 110), k);
                roundRect(b, irr::core::rect<irr::f32>(c.X - 6, c.Y - 2, c.X + 6, c.Y + 7), 2, lockCol, lockCol);
                b.sector(irr::core::vector2df(c.X, c.Y - 3), 2.6f, 4.3f, -90, 90, lockCol, lockCol);
                b.rect(irr::core::rect<irr::f32>(c.X - 4.3f, c.Y - 3, c.X - 2.6f, c.Y - 1), lockCol);
                b.rect(irr::core::rect<irr::f32>(c.X + 2.6f, c.Y - 3, c.X + 4.3f, c.Y - 1), lockCol);
            }
        }
        else {
            const irr::f32 badge = 23.0f;
            iconCentre = irr::core::vector2df(r.UpperLeftCorner.X + 22 + badge, r.UpperLeftCorner.Y + 22 + badge);
            const irr::video::SColor badgeCol = (style == Tile_Primary) ? irr::video::SColor(48, 255, 255, 255)
                : irr::video::SColor((irr::u32)(40 + 40 * k), 64, 156, 240);
            b.disc(iconCentre, badge, badgeCol, badgeCol);
            drawIcon(b, icon, iconCentre, 14.5f, iconCol);
            //Arrow on hover: "go".
            if (k > 0.01f) {
                const irr::core::vector2df a(r.LowerRightCorner.X - 26 + 4 * k, r.UpperLeftCorner.Y + 22 + badge);
                const irr::video::SColor ac((irr::u32)(255 * k), 255, 255, 255);
                b.line(irr::core::vector2df(a.X - 12, a.Y), a, 2.0f, ac);
                b.line(irr::core::vector2df(a.X - 6, a.Y - 6), a, 2.0f, ac);
                b.line(irr::core::vector2df(a.X - 6, a.Y + 6), a, 2.0f, ac);
            }
        }
        b.flush();

        //Text
        const irr::core::rect<irr::s32> clip = AbsoluteClippingRect;
        if (chip) {
            if (titleFont) {
                const irr::core::dimension2du d = titleFont->getDimension(title.c_str());
                const irr::s32 x = (irr::s32)r.UpperLeftCorner.X + 42;
                const irr::s32 y = (irr::s32)r.getCenter().Y - (irr::s32)d.Height / 2;
                titleFont->draw(title.c_str(), irr::core::rect<irr::s32>(x, y, x + (irr::s32)d.Width + 2, y + (irr::s32)d.Height),
                    mixColour(Theme::textDim, Theme::text, 0.55f + 0.45f * k), false, false, &clip);
            }
        }
        else {
            irr::s32 y = (irr::s32)(r.UpperLeftCorner.Y + 22 + 46 + 14);
            const irr::s32 x = (irr::s32)r.UpperLeftCorner.X + 22;
            const irr::s32 w = (irr::s32)r.getWidth() - 44;
            if (titleFont) {
                const irr::core::dimension2du d = titleFont->getDimension(title.c_str());
                titleFont->draw(title.c_str(), irr::core::rect<irr::s32>(x, y, x + w, y + (irr::s32)d.Height), Theme::text, false, false, &clip);
                y += (irr::s32)d.Height + 4;
            }
            if (subFont) {
                const std::vector<std::wstring> lines = wrapText(subFont, subtitle, w);
                for (size_t i = 0; i < lines.size() && i < 3; i++) {
                    const irr::core::dimension2du d = subFont->getDimension(lines[i].c_str());
                    if (y + (irr::s32)d.Height > (irr::s32)r.LowerRightCorner.Y - 8) { break; } //no room left in the card
                    subFont->draw(lines[i].c_str(), irr::core::rect<irr::s32>(x, y, x + w, y + (irr::s32)d.Height),
                        style == Tile_Primary ? irr::video::SColor(255, 214, 230, 248) : Theme::textDim, false, false, &clip);
                    y += (irr::s32)d.Height + 1;
                }
            }
        }
    }

private:
    void fire()
    {
        if (!Parent) { return; }
        irr::SEvent e;
        e.EventType = irr::EET_GUI_EVENT;
        e.GUIEvent.Caller = this;
        e.GUIEvent.Element = 0;
        e.GUIEvent.EventType = irr::gui::EGET_BUTTON_CLICKED;
        Parent->OnEvent(e);
    }

    std::wstring title, subtitle;
    LauncherIcon icon;
    TileStyle style;
    bool hovered, pressed, locked;
    irr::f32 hover;
    irr::u32 lastMs;
    irr::gui::IGUIFont* titleFont;
    irr::gui::IGUIFont* subFont;
};

//Background picture scaled to cover the window, plus the gradient that the cards sit on.
void drawBackdrop(irr::video::IVideoDriver* driver, irr::video::ITexture* bg, const irr::core::dimension2du& screen)
{
    const irr::s32 W = (irr::s32)screen.Width, H = (irr::s32)screen.Height;
    driver->draw2DRectangle(irr::video::SColor(255, 8, 18, 34), irr::core::rect<irr::s32>(0, 0, W, H));
    if (bg) {
        const irr::core::dimension2du ts = bg->getOriginalSize();
        const irr::f32 scale = irr::core::max_((irr::f32)W / ts.Width, (irr::f32)H / ts.Height);
        const irr::f32 sw = W / scale, sh = H / scale;
        const irr::f32 sx = (ts.Width - sw) * 0.5f, sy = (ts.Height - sh) * 0.5f;
        driver->getMaterial2D().TextureLayer[0].BilinearFilter = true;
        driver->getMaterial2D().TextureLayer[0].TrilinearFilter = true;
        driver->enableMaterial2D(true);
        driver->draw2DImage(bg, irr::core::rect<irr::s32>(0, 0, W, H),
            irr::core::rect<irr::s32>((irr::s32)sx, (irr::s32)sy, (irr::s32)(sx + sw), (irr::s32)(sy + sh)));
        driver->enableMaterial2D(false);
    }
    //Fade to deep navy behind the controls.
    const irr::video::SColor clear(0, Theme::shade.getRed(), Theme::shade.getGreen(), Theme::shade.getBlue());
    const irr::video::SColor mid(150, Theme::shade.getRed(), Theme::shade.getGreen(), Theme::shade.getBlue());
    const irr::video::SColor deep(232, Theme::shade.getRed(), Theme::shade.getGreen(), Theme::shade.getBlue());
    const irr::s32 y0 = (irr::s32)(H * 0.42f), y1 = (irr::s32)(H * 0.64f);
    driver->draw2DRectangle(irr::core::rect<irr::s32>(0, y0, W, y1), clear, clear, mid, mid);
    driver->draw2DRectangle(irr::core::rect<irr::s32>(0, y1, W, H), mid, mid, deep, deep);
}

//Toast shown for a moment after an application is launched.
std::wstring g_toastText;
irr::u32 g_toastStartMs = 0;

void drawToast(irr::video::IVideoDriver* driver, irr::gui::IGUIFont* font, const irr::core::dimension2du& screen, irr::s32 y)
{
    if (g_toastText.empty() || !font || !g_device) { return; }
    const irr::u32 age = g_device->getTimer()->getRealTime() - g_toastStartMs;
    if (age > 3000) { g_toastText.clear(); return; }
    const irr::f32 alpha = (age < 200) ? age / 200.0f : (age > 2400 ? (3000 - age) / 600.0f : 1.0f);
    const irr::core::dimension2du d = font->getDimension(g_toastText.c_str());
    const irr::f32 w = (irr::f32)d.Width + 48, h = (irr::f32)d.Height + 18;
    const irr::f32 x = (screen.Width - w) * 0.5f;
    irr::gui::PanelBatch b;
    b.begin(driver);
    roundRect(b, irr::core::rect<irr::f32>(x, (irr::f32)y, x + w, y + h), h * 0.5f,
        irr::video::SColor((irr::u32)(215 * alpha), 22, 96, 190), irr::video::SColor((irr::u32)(215 * alpha), 16, 74, 158));
    b.flush();
    font->draw(g_toastText.c_str(), irr::core::rect<irr::s32>((irr::s32)x + 24, y + 9, (irr::s32)(x + w), y + 9 + (irr::s32)d.Height),
        irr::video::SColor((irr::u32)(255 * alpha), 255, 255, 255), false, false);
}

//Toast for an application being started.
void showLaunchToast(const std::wstring& title)
{
    if (!g_device) { return; }
    g_toastText = std::wstring(L"Lancement : ") + title + L"...";
    g_toastStartMs = g_device->getTimer()->getRealTime();
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

                //KYARA TOUCHES: handled here, ABOVE the fork() below - this only opens the sheet.
                if (id == KEYS_BUTTON) {
                    if (g_keySheet) { g_keySheet->open(); }
                    return true;
                }

                //Simulator: may first ask which screen to use (see startSimulator).
                if (id == BC_BUTTON) {
                    g_simulatorTitle = event.GUIEvent.Caller->getText();
                    startSimulator();
                    return true;
                }

                //Gyro repeater: may first ask which screen to use (see startRepeater).
                if (id == RP_BUTTON) {
                    g_repeaterTitle = event.GUIEvent.Caller->getText();
                    startRepeater();
                    return true;
                }

                //Feedback while the application starts (it can take a few seconds).
                if (id != DOC_BUTTON && id != USER_BUTTON && g_device) {
                    showLaunchToast(event.GUIEvent.Caller->getText());
                }

#ifndef _WIN32
                int pid = fork();  // posix only (GNU/Linux, MacOS)
                if (pid > 0) return false;
#endif

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
            //KYARA TOUCHES: while the sheet (or the screen picker) is open, escape is theirs: it closes
            //them, and must not shut the launcher down by surprise.
            if (overlayOpen()) {
                return false;
            }
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
    const bool french = (modifier == "fr");
    g_french = french;

    float fontScale = IniFile::iniFileTof32(iniFilename, "font_scale");
    if (fontScale < 1) {
        fontScale = 1.0;
    }

    //Window: 1280 x 720, or 90 % of a smaller desktop. Resizable: the layout follows.
    irr::u32 graphicsWidth = 1280;
    irr::u32 graphicsHeight = 720;
    {
        irr::IrrlichtDevice* nulldevice = irr::createDevice(irr::video::EDT_NULL);
        if (nulldevice) {
            const irr::core::dimension2du desk = nulldevice->getVideoModeList()->getDesktopResolution();
            nulldevice->drop();
            if (desk.Width > 0 && graphicsWidth > desk.Width * 0.9f) { graphicsWidth = (irr::u32)(desk.Width * 0.9f); }
            if (desk.Height > 0 && graphicsHeight > desk.Height * 0.9f) { graphicsHeight = (irr::u32)(desk.Height * 0.9f); }
        }
    }
    irr::u32 graphicsDepth = 32;
    bool fullScreen = false;

    irr::IrrlichtDevice* device = irr::createDevice(irr::video::EDT_OPENGL, irr::core::dimension2d<irr::u32>(graphicsWidth, graphicsHeight), graphicsDepth, fullScreen, false, false, 0);
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::gui::IGUIEnvironment* env = device->getGUIEnvironment();
    g_device = device; //KYARA TOUCHES
    device->setResizable(true);


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

    //Fonts: the simulator's font (bc5.ini font=, normally noto-sans), in a few sizes.
    std::string fontName = IniFile::iniFileToString(iniFilename, "font");
    if (fontName.empty()) { fontName = "noto-sans"; }
    auto loadFont = [&](int size) -> irr::gui::IGUIFont* {
        size = (int)(size * fontScale + 0.5f);
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

    //Background picture: bc5.ini launcher_image=<file> (in media/ or a path), else the usual one.
    std::string bgName = IniFile::iniFileToString(iniFilename, "launcher_image");
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

    //Main applications: cards. The simulator is the primary action.
    std::vector<LauncherTile*> cards;
    cards.push_back(new LauncherTile(env, BC_BUTTON, T("startBC"),
        tr("startBCInfo", L"Lancer un exercice : passerelle, radar et instruments", L"Run an exercise: bridge view, radar and instruments"), Icon_Helm, Tile_Primary));
    cards.push_back(new LauncherTile(env, RP_BUTTON, T("startRP"),
        tr("startRPInfo", L"Cap du navire sur un \u00E9cran \u00E0 part (ou angle de barre)", L"Ship's heading on a screen of its own (or rudder angle)"), Icon_Compass, Tile_Card));
    cards.push_back(new LauncherTile(env, ED_BUTTON, T("startED"),
        tr("startEDInfo", L"Cr\u00E9er et modifier les exercices de navigation", L"Create and edit navigation exercises"), Icon_Route, Tile_Card));
    cards.push_back(new LauncherTile(env, FE_BUTTON, T("startFE"),
        tr("startFEInfo", L"Incendie \u00E0 bord, naufrag\u00E9s et moyens SAR", L"Fire on board, survivors and SAR units"), Icon_Flame, Tile_Card));
    cards.push_back(new LauncherTile(env, MH_BUTTON, T("startMH"),
        tr("startMHInfo", L"Relier plusieurs postes pour un exercice commun", L"Link several stations in one exercise"), Icon_Network, Tile_Card));

    //Settings and tools: chips.
    std::vector<LauncherTile*> chips;
    chips.push_back(new LauncherTile(env, INI_BC_BUTTON, T("startINIBC"), L"", Icon_Gear, Tile_Chip));
    chips.push_back(new LauncherTile(env, INI_MC_BUTTON, T("startINIMC"), L"", Icon_Gear, Tile_Chip));
    chips.push_back(new LauncherTile(env, INI_MH_BUTTON, T("startINIMH"), L"", Icon_Gear, Tile_Chip));
    chips.push_back(new LauncherTile(env, KEYS_BUTTON, french ? L"Raccourcis clavier" : L"Keyboard shortcuts", L"", Icon_Keys, Tile_Chip));
    LauncherTile* exitTile = new LauncherTile(env, EXIT_BUTTON, T("leave"), L"", Icon_Power, Tile_ChipDanger);

    std::vector<LauncherTile*> all(cards);
    all.insert(all.end(), chips.begin(), chips.end());
    all.push_back(exitTile);
    for (size_t i = 0; i < cards.size(); i++) { cards[i]->setFonts(titleFont, textFont); }
    for (size_t i = 0; i < chips.size(); i++) { chips[i]->setFonts(textFont, 0); }
    exitTile->setFonts(textFont, 0);
    chips[3]->setToolTipText(french ? L"Liste des touches du simulateur (FR / EN)" : L"Simulator keys (FR / EN)");
    //The three settings tools ask for the administrator password.
    for (int i = 0; i < 3; i++) {
        chips[i]->setLocked(true);
        chips[i]->setToolTipText(french ? L"R\u00E9serv\u00E9 aux administrateurs : mot de passe demand\u00E9" : L"Administrators only: asks for the password");
    }
    for (size_t i = 0; i < all.size(); i++) { all[i]->drop(); } //the GUI tree holds them

    //Overlays, created after the tiles so that they lie on top of them.
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

    //Card titles: the biggest of these that fits every card (a narrow window gets smaller lettering).
    irr::gui::IGUIFont* cardTitleFonts[5] = { titleFont, loadFont(19), loadFont(18), loadFont(17), loadFont(16) };

    irr::s32 toastY = 0;
    irr::core::dimension2du laidOut(0, 0);
    auto layout = [&](const irr::core::dimension2du& scr) {
        const irr::s32 W = (irr::s32)scr.Width, H = (irr::s32)scr.Height;
        const irr::s32 margin = irr::core::max_(28, (irr::s32)(W * 0.035f));
        const irr::s32 gap = irr::core::max_(12, (irr::s32)(W * 0.013f));
        const irr::s32 chipH = (irr::s32)(42 * fontScale);
        const irr::s32 footerH = 34;
        const irr::s32 chipY = H - footerH - chipH - 14;
        const irr::s32 cardCount = (irr::s32)cards.size();
        const irr::s32 cardW = (W - 2 * margin - (cardCount - 1) * gap) / cardCount;
        int pick = 4;
        for (int f = 0; f < 5; f++) {
            if (!cardTitleFonts[f]) { continue; }
            irr::s32 widest = 0;
            for (size_t i = 0; i < cards.size(); i++) {
                widest = irr::core::max_(widest, (irr::s32)cardTitleFonts[f]->getDimension(cards[i]->getText()).Width);
            }
            if (widest <= cardW - 44) { pick = f; break; }
        }
        irr::gui::IGUIFont* cardTitle = cardTitleFonts[pick] ? cardTitleFonts[pick] : titleFont;
        //Tall enough for every card's description (narrow cards wrap it onto more lines).
        irr::s32 cardH = irr::core::clamp((irr::s32)(H * 0.23f), 150, 200);
        for (irr::s32 i = 0; i < cardCount; i++) {
            cards[i]->setFonts(cardTitle, pick <= 2 ? textFont : smallFont);
            cardH = irr::core::max_(cardH, cards[i]->neededCardHeight(cardW));
        }
        cardH = irr::core::min_(cardH, 230);
        const irr::s32 cardY = chipY - 22 - cardH;
        for (irr::s32 i = 0; i < cardCount; i++) {
            const irr::s32 x = margin + i * (cardW + gap);
            cards[i]->setRelativePosition(irr::core::rect<irr::s32>(x, cardY, x + cardW, cardY + cardH));
        }
        irr::s32 x = margin;
        for (size_t i = 0; i < chips.size(); i++) {
            const irr::s32 w = chips[i]->preferredChipWidth();
            chips[i]->setRelativePosition(irr::core::rect<irr::s32>(x, chipY, x + w, chipY + chipH));
            x += w + 12;
        }
        const irr::s32 ew = exitTile->preferredChipWidth();
        exitTile->setRelativePosition(irr::core::rect<irr::s32>(W - margin - ew, chipY, W - margin, chipY + chipH));
        toastY = cardY - 64;
        laidOut = scr;
        };

    env->setFocus(cards[0]);

    //Footer text
    std::wstring footer = L"NAUTITECH  \u00B7  Simulateur de Navigation Maritime";
    if (!LONGVERSION.empty()) {
        footer += L"  \u00B7  v";
        footer += irr::core::stringw(LONGVERSION.c_str()).c_str();
    }

    Receiver receiver;
    device->setEventReceiver(&receiver);

#ifdef FOR_DEB
    chdir("/usr/bin");
#endif // FOR_DEB

    // Render loop
    while (device->run()) {
        const irr::core::dimension2du screen = driver->getScreenSize();
        if (screen != laidOut) { layout(screen); }

        driver->beginScene(irr::video::ECBF_COLOR | irr::video::ECBF_DEPTH, irr::video::SColor(255, 8, 18, 34));
        drawBackdrop(driver, bgTex, screen);
        env->drawAll();
        drawToast(driver, textFont, screen, toastY);
        if (smallFont) {
            const irr::core::dimension2du d = smallFont->getDimension(footer.c_str());
            const irr::s32 margin = irr::core::max_(28, (irr::s32)(screen.Width * 0.04f));
            smallFont->draw(footer.c_str(), irr::core::rect<irr::s32>(margin, (irr::s32)screen.Height - 26, margin + (irr::s32)d.Width + 2, (irr::s32)screen.Height - 26 + (irr::s32)d.Height),
                irr::video::SColor(170, 200, 214, 230), false, false);
        }
        driver->endScene();

        //Smooth while something moves, light on the CPU otherwise.
        bool animating = !g_toastText.empty();
        for (size_t i = 0; i < all.size() && !animating; i++) { animating = all[i]->isAnimating(); }
        device->sleep(animating ? 15 : 40);
#ifndef _WIN32
        while (waitpid(-1, 0, WNOHANG) > 0) {} //applications started and since closed
#endif
    }

    return EXIT_SUCCESS;
}
