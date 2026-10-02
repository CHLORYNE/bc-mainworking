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

    //Key column width in pixels (the font is proportional, so pad by measured width, not characters).
    irr::gui::IGUIFont* listFont = env->getSkin() ? env->getSkin()->getFont() : 0;
    irr::u32 keyColumn = 0;
    for (int i = 0; i < KEY_ROW_COUNT && listFont; i++) {
        if (KEY_ROWS[i].keys) {
            const irr::u32 w = listFont->getDimension(KEY_ROWS[i].keys).Width;
            if (w > keyColumn) { keyColumn = w; }
        }
    }
    keyColumn += 28;

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
            if (listFont) {
                while (listFont->getDimension(keys.c_str()).Width < keyColumn) { keys += L" "; }
            }
            else {
                while (keys.size() < 26) { keys += L" "; }
            }
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

enum LauncherIcon { Icon_Helm, Icon_Route, Icon_Flame, Icon_Network, Icon_Gear, Icon_Keys, Icon_Power };

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
        title(title), subtitle(subtitle), icon(icon), style(style), hovered(false), pressed(false), hover(0), lastMs(0),
        titleFont(0), subFont(0)
    {
        setTabStop(true);
        setText(title.c_str());
    }

    void setFonts(irr::gui::IGUIFont* t, irr::gui::IGUIFont* s) { titleFont = t; subFont = s; }

    //Width a chip needs for its label.
    irr::s32 preferredChipWidth() const
    {
        const irr::s32 tw = titleFont ? (irr::s32)titleFont->getDimension(title.c_str()).Width : 100;
        return tw + 62;
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
                for (size_t i = 0; i < lines.size() && i < 2; i++) {
                    const irr::core::dimension2du d = subFont->getDimension(lines[i].c_str());
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
    bool hovered, pressed;
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

                //Feedback while the application starts (it can take a few seconds).
                if (id != DOC_BUTTON && id != USER_BUTTON && g_device) {
                    g_toastText = std::wstring(L"Lancement : ") + event.GUIEvent.Caller->getText() + L"...";
                    g_toastStartMs = g_device->getTimer()->getRealTime();
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
    const bool french = (modifier == "fr");

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
    for (size_t i = 0; i < all.size(); i++) { all[i]->drop(); } //the GUI tree holds them

    irr::s32 toastY = 0;
    irr::core::dimension2du laidOut(0, 0);
    auto layout = [&](const irr::core::dimension2du& scr) {
        const irr::s32 W = (irr::s32)scr.Width, H = (irr::s32)scr.Height;
        const irr::s32 margin = irr::core::max_(28, (irr::s32)(W * 0.04f));
        const irr::s32 gap = irr::core::max_(14, (irr::s32)(W * 0.016f));
        const irr::s32 cardH = irr::core::clamp((irr::s32)(H * 0.23f), 150, 200);
        const irr::s32 chipH = (irr::s32)(42 * fontScale);
        const irr::s32 footerH = 34;
        const irr::s32 chipY = H - footerH - chipH - 14;
        const irr::s32 cardY = chipY - 22 - cardH;
        const irr::s32 cardW = (W - 2 * margin - 3 * gap) / 4;
        for (int i = 0; i < 4; i++) {
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
    }

    return EXIT_SUCCESS;
}
