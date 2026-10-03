/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2014 James Packer

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

#include "ScenarioChoice.hpp"
#include "StartupEventReceiver.hpp"
#include "Constants.hpp"
#include "ExitMessage.hpp"
#include "IniFile.hpp"
#include "Utilities.hpp"
#include "ScenarioDataStructure.hpp"
#include "IncidentConfig.hpp"
#include "GUIPanelDraw.hpp"
#include "UiTheme.hpp"
#include "chartView/ChartView.hpp"
#include "chartView/ChartDraw.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace {

const irr::s32 ID_SEARCH = 601, ID_HOSTNAME = 602, ID_PORT = 603, ID_START = 604, ID_QUIT = 605;
const irr::s32 ID_MODE_NORMAL = 606, ID_MODE_SECONDARY = 607, ID_MODE_MULTIPLAYER = 608;
const irr::s32 ID_ZOOM_IN = 609, ID_ZOOM_OUT = 610, ID_ZOOM_FIT = 611;
const double kPi = 3.14159265358979323846;

//---------------------------------------------------------------------------------------------------
//Text and number formatting
//---------------------------------------------------------------------------------------------------

//Text from a file or a folder name: UTF-8 when it reads as UTF-8, else the Windows code page (Latin-1).
std::wstring decodeText(const std::string& s)
{
    bool multi = false, valid = true;
    for (size_t i = 0; i < s.size() && valid;) {
        const unsigned char c = (unsigned char)s[i];
        const int n = (c < 0x80) ? 0 : ((c >> 5) == 6) ? 1 : ((c >> 4) == 14) ? 2 : ((c >> 3) == 30) ? 3 : -1;
        if (n < 0) { valid = false; break; }
        if (n > 0) { multi = true; }
        for (int k = 1; k <= n; k++) {
            if (i + k >= s.size() || (((unsigned char)s[i + k]) & 0xC0) != 0x80) { valid = false; break; }
        }
        i += n + 1;
    }
    std::wstring out;
    if (valid && multi) {
        for (size_t i = 0; i < s.size();) {
            const unsigned char c = (unsigned char)s[i];
            unsigned long cp = c;
            int n = 0;
            if (c >= 0xF0) { cp = c & 0x07; n = 3; }
            else if (c >= 0xE0) { cp = c & 0x0F; n = 2; }
            else if (c >= 0xC0) { cp = c & 0x1F; n = 1; }
            for (int k = 1; k <= n; k++) { cp = (cp << 6) | (((unsigned char)s[i + k]) & 0x3F); }
            out += (cp <= 0xFFFF) ? (wchar_t)cp : L'?';
            i += n + 1;
        }
    }
    else {
        for (size_t i = 0; i < s.size(); i++) { out += (wchar_t)(unsigned char)s[i]; }
    }
    return out;
}

std::wstring lower(std::wstring s)
{
    for (size_t i = 0; i < s.size(); i++) { s[i] = (wchar_t)std::towlower(s[i]); }
    return s;
}

std::wstring trimText(const std::wstring& s)
{
    const size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) { return L""; }
    const size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}

//Model names are folder names: underscores read as spaces.
std::wstring shipLabel(const std::string& model)
{
    std::wstring w = decodeText(model);
    std::replace(w.begin(), w.end(), L'_', L' ');
    return w;
}

std::wstring number(irr::f32 v, int decimals, bool french)
{
    wchar_t buf[32];
    swprintf(buf, 32, decimals == 0 ? L"%.0f" : (decimals == 1 ? L"%.1f" : L"%.2f"), v);
    std::wstring s = buf;
    if (french) { std::replace(s.begin(), s.end(), L'.', L','); }
    return s;
}

std::wstring clockTime(irr::f32 hours)
{
    while (hours < 0) { hours += 24; }
    int minutes = (int)std::floor(hours * 60.0f + 0.5f) % (24 * 60);
    wchar_t buf[16];
    swprintf(buf, 16, L"%02d:%02d", minutes / 60, minutes % 60);
    return buf;
}

std::wstring dateText(irr::u32 day, irr::u32 month, irr::u32 year, bool french)
{
    static const wchar_t* fr[12] = { L"janv.", L"f\u00E9vr.", L"mars", L"avr.", L"mai", L"juin", L"juil.", L"ao\u00FBt", L"sept.", L"oct.", L"nov.", L"d\u00E9c." };
    static const wchar_t* en[12] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };
    if (month < 1 || month > 12 || day == 0) { return french ? L"date non pr\u00E9cis\u00E9e" : L"no date set"; }
    return std::to_wstring(day) + L" " + (french ? fr[month - 1] : en[month - 1]) + L" " + std::to_wstring(year);
}

std::wstring position(irr::f32 value, bool latitude)
{
    const irr::f32 v = std::fabs(value);
    int deg = (int)v;
    irr::f32 minutes = (v - deg) * 60.0f;
    if (minutes > 59.995f) { deg++; minutes = 0; }
    wchar_t buf[32];
    swprintf(buf, 32, latitude ? L"%02d\u00B0%05.2f'%lc" : L"%03d\u00B0%05.2f'%lc", deg, minutes,
        latitude ? (value >= 0 ? L'N' : L'S') : (value >= 0 ? L'E' : L'W'));
    return buf;
}

std::wstring compassPoint(irr::f32 degrees, bool french)
{
    static const wchar_t* fr[16] = { L"N", L"NNE", L"NE", L"ENE", L"E", L"ESE", L"SE", L"SSE", L"S", L"SSO", L"SO", L"OSO", L"O", L"ONO", L"NO", L"NNO" };
    static const wchar_t* en[16] = { L"N", L"NNE", L"NE", L"ENE", L"E", L"ESE", L"SE", L"SSE", L"S", L"SSW", L"SW", L"WSW", L"W", L"WNW", L"NW", L"NNW" };
    int i = (int)std::floor(std::fmod(std::fmod(degrees, 360.0f) + 360.0f, 360.0f) / 22.5f + 0.5f) % 16;
    return french ? fr[i] : en[i];
}

std::wstring heading3(irr::f32 degrees)
{
    wchar_t buf[16];
    swprintf(buf, 16, L"%03.0f\u00B0", std::fmod(std::fmod(degrees, 360.0f) + 360.0f, 360.0f));
    return buf;
}

//---------------------------------------------------------------------------------------------------
//The exercises
//---------------------------------------------------------------------------------------------------

struct OwnShipFacts {
    bool known;
    irr::f32 tonnes, maxSpeed;
    bool twin, azimuth, bowThruster, fireFighting;
    OwnShipFacts() : known(false), tonnes(0), maxSpeed(0), twin(false), azimuth(false), bowThruster(false), fireFighting(false) {}
};

struct Exercise {
    std::string folder;
    std::wstring name;
    std::wstring description;
    std::wstring searchText;   //lower-case name and area, for the search box
    ScenarioData data;
    bool hasIncident;
    IncidentConfig incident;
    OwnShipFacts ownShip;
};

std::string readWholeFile(const std::string& path)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) { return ""; }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) { s.erase(0, 3); }
    return s;
}

OwnShipFacts ownShipFacts(const std::string& model)
{
    OwnShipFacts facts;
    std::string ini = Utilities::getUserDir() + "Models/Ownship/" + model + "/boat.ini";
    if (!Utilities::pathExists(ini)) { ini = "Models/Ownship/" + model + "/boat.ini"; }
    if (model.empty() || !Utilities::pathExists(ini)) { return facts; }
    facts.known = true;
    facts.tonnes = IniFile::iniFileTof32(ini, "Mass") / 1000.0f;
    facts.maxSpeed = IniFile::iniFileTof32(ini, "maxSpeedAhead");
    facts.twin = IniFile::iniFileTof32(ini, "PropSpace") != 0;
    facts.azimuth = IniFile::iniFileTou32(ini, "AzimuthDrive") == 1;
    facts.bowThruster = IniFile::iniFileTof32(ini, "BowThrusterForce") > 0;
    facts.fireFighting = IniFile::iniFileTou32(ini, "FireFighting") == 1;
    return facts;
}

//Every exercise folder (not the _mp multiplayer ones), sorted by name.
std::vector<Exercise> loadExercises(irr::IrrlichtDevice* device, const std::string& scenarioPath)
{
    std::vector<Exercise> list;
    irr::io::IFileSystem* fileSystem = device->getFileSystem();
    if (!fileSystem) { ExitMessage::exitWithMessage("Could not get file system access."); }
    const irr::io::path cwd = fileSystem->getWorkingDirectory();
    if (!fileSystem->changeWorkingDirectoryTo(scenarioPath.c_str())) {
        ExitMessage::exitWithMessage("Could not get change working directory to scenario directory.");
    }
    irr::io::IFileList* fileList = fileSystem->createFileList();
    std::vector<std::string> folders;
    if (fileList) {
        for (irr::u32 i = 0; i < fileList->getFileCount(); i++) {
            if (!fileList->isDirectory(i)) { continue; }
            const std::string name = fileList->getFileName(i).c_str();
            if (name.empty() || name[0] == '.') { continue; } //., .. and hidden folders
            if (name.size() >= 3 && name.compare(name.size() - 3, 3, "_mp") == 0) { continue; } //multiplayer exercises
            folders.push_back(name);
        }
        fileList->drop();
    }
    if (!fileSystem->changeWorkingDirectoryTo(cwd)) {
        ExitMessage::exitWithMessage("Can't return to normal working directory.");
    }

    for (size_t i = 0; i < folders.size(); i++) {
        Exercise e;
        e.folder = folders[i];
        e.name = decodeText(folders[i]);
        const std::string dir = scenarioPath + folders[i];
        e.description = trimText(decodeText(readWholeFile(dir + "/description.ini")));
        e.data = Utilities::getScenarioDataFromFile(dir, folders[i]);
        e.hasIncident = e.incident.load(dir + "/incident.ini");
        e.ownShip = ownShipFacts(e.data.ownShipData.ownShipName);
        e.searchText = lower(e.name + L" " + decodeText(e.data.worldName));
        list.push_back(e);
    }
    std::sort(list.begin(), list.end(), [](const Exercise& a, const Exercise& b) { return lower(a.name) < lower(b.name); });
    return list;
}

//---------------------------------------------------------------------------------------------------
//Drawing helpers
//---------------------------------------------------------------------------------------------------

//Line segment clipped to a rectangle (Liang-Barsky); false if nothing of it is inside.
bool clipSegment(irr::core::vector2df& a, irr::core::vector2df& b, const irr::core::rect<irr::f32>& r)
{
    irr::f32 t0 = 0, t1 = 1;
    const irr::f32 dx = b.X - a.X, dy = b.Y - a.Y;
    const irr::f32 p[4] = { -dx, dx, -dy, dy };
    const irr::f32 q[4] = { a.X - r.UpperLeftCorner.X, r.LowerRightCorner.X - a.X, a.Y - r.UpperLeftCorner.Y, r.LowerRightCorner.Y - a.Y };
    for (int i = 0; i < 4; i++) {
        if (p[i] == 0) {
            if (q[i] < 0) { return false; }
            continue;
        }
        const irr::f32 t = q[i] / p[i];
        if (p[i] < 0) { if (t > t1) { return false; } if (t > t0) { t0 = t; } }
        else { if (t < t0) { return false; } if (t < t1) { t1 = t; } }
    }
    const irr::core::vector2df a0 = a;
    a = a0 + irr::core::vector2df(dx * t0, dy * t0);
    b = a0 + irr::core::vector2df(dx * t1, dy * t1);
    return true;
}

//Small rounded label ("pill").
irr::f32 pill(irr::gui::PanelBatch& b, irr::gui::IGUIFont* font, const std::wstring& text, irr::f32 x, irr::f32 y, irr::f32 h,
    irr::video::SColor fill, irr::video::SColor edge)
{
    const irr::f32 w = Ui::textWidth(font, text) + h * 0.9f;
    const irr::core::rect<irr::f32> r(x, y, x + w, y + h);
    Ui::roundRect(b, r, h * 0.5f, fill, fill);
    Ui::roundRectOutline(b, r, h * 0.5f, 1.0f, edge);
    return w;
}

//---------------------------------------------------------------------------------------------------
//The screen
//---------------------------------------------------------------------------------------------------

struct Fonts {
    irr::gui::IGUIFont* title;
    irr::gui::IGUIFont* heading;
    irr::gui::IGUIFont* name;
    irr::gui::IGUIFont* text;
    irr::gui::IGUIFont* small;
    irr::gui::IGUIFont* tiny;
};

class ScenarioScreen : public irr::gui::IGUIElement, public StartupScreen
{
public:
    enum Mode { ModeExercise = 0, ModeSecondary = 1, ModeMultiplayer = 2 };

    ScenarioScreen(irr::IrrlichtDevice* device, std::vector<Exercise>& exercises, bool french, const std::string& fontName,
        const std::wstring& hostname, irr::u32 udpPort, const std::string& lastExercise)
        : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, device->getGUIEnvironment(), device->getGUIEnvironment()->getRootGUIElement(), -1,
            irr::core::rect<irr::s32>(0, 0, 10, 10)),
        device(device), exercises(exercises), french(french), fontName(fontName),
        mode(ModeExercise), selected(-1), hovered(-1), scroll(0), started(false), mapFor(-1), chartPending(false),
        dragging(false), lastClickMs(0), lastClickRow(-1), laidOutFor(0, 0)
    {
        irr::gui::IGUIEnvironment* env = Environment;
        search = env->addEditBox(L"", irr::core::rect<irr::s32>(0, 0, 10, 10), false, this, ID_SEARCH);
        hostnameBox = env->addEditBox(hostname.c_str(), irr::core::rect<irr::s32>(0, 0, 10, 10), false, this, ID_HOSTNAME);
        portBox = env->addEditBox(std::to_wstring(udpPort).c_str(), irr::core::rect<irr::s32>(0, 0, 10, 10), false, this, ID_PORT);
        irr::gui::IGUIEditBox* boxes[3] = { search, hostnameBox, portBox };
        for (int i = 0; i < 3; i++) {
            boxes[i]->setDrawBackground(false);
            boxes[i]->setCursorChar(L'|');
            boxes[i]->setOverrideColor(Ui::text);
            boxes[i]->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
        }
        hostnameBox->setToolTipText(french ? L"Noms ou adresses des autres postes (r\u00E9p\u00E9titeur, affichages secondaires, contr\u00F4leur), s\u00E9par\u00E9s par des virgules"
            : L"Names or addresses of the other stations (repeater, secondary displays, controller), separated by commas");

        const wchar_t* modeLabels[3] = { french ? L"Exercice" : L"Exercise", french ? L"Affichage secondaire" : L"Secondary display",
            french ? L"Multijoueur" : L"Multiplayer" };
        const irr::s32 modeIds[3] = { ID_MODE_NORMAL, ID_MODE_SECONDARY, ID_MODE_MULTIPLAYER };
        for (int i = 0; i < 3; i++) {
            modeButtons[i] = new Ui::Button(env, this, modeIds[i], irr::core::rect<irr::s32>(0, 0, 10, 10), modeLabels[i], Ui::Button::Quiet);
            modeButtons[i]->drop();
        }
        startButton = new Ui::Button(env, this, ID_START, irr::core::rect<irr::s32>(0, 0, 10, 10), L"", Ui::Button::Primary);
        startButton->drop();
        quitButton = new Ui::Button(env, this, ID_QUIT, irr::core::rect<irr::s32>(0, 0, 10, 10), french ? L"Quitter" : L"Quit", Ui::Button::Secondary);
        quitButton->drop();
        zoomIn = new Ui::Button(env, this, ID_ZOOM_IN, irr::core::rect<irr::s32>(0, 0, 10, 10), L"+", Ui::Button::Secondary);
        zoomIn->setGlyph(Ui::Button::Plus);
        zoomIn->setToolTipText(french ? L"Zoom avant (molette)" : L"Zoom in (mouse wheel)");
        zoomIn->drop();
        zoomOut = new Ui::Button(env, this, ID_ZOOM_OUT, irr::core::rect<irr::s32>(0, 0, 10, 10), L"-", Ui::Button::Secondary);
        zoomOut->setGlyph(Ui::Button::Minus);
        zoomOut->setToolTipText(french ? L"Zoom arri\u00E8re (molette)" : L"Zoom out (mouse wheel)");
        zoomOut->drop();
        zoomFit = new Ui::Button(env, this, ID_ZOOM_FIT, irr::core::rect<irr::s32>(0, 0, 10, 10), french ? L"Recadrer" : L"Fit", Ui::Button::Secondary);
        zoomFit->setToolTipText(french ? L"Revenir \u00E0 la vue de l'exercice (double-clic sur la carte)" : L"Back to the exercise view (double-click the map)");
        zoomFit->drop();

        filter();
        //The last exercise started from this screen, else the first.
        for (size_t i = 0; i < shown.size(); i++) {
            if (exercises[shown[i]].folder == lastExercise) { selected = (int)i; }
        }
        if (selected < 0 && !shown.empty()) { selected = 0; }
        setMode(ModeExercise);
        Environment->setFocus(search);
    }

    ~ScenarioScreen()
    {
        for (std::map<std::string, ChartView*>::iterator it = charts.begin(); it != charts.end(); ++it) { delete it->second; }
    }

    bool isStarted() const { return started; }
    Mode getMode() const { return mode; }
    //Index in the exercise list of the one chosen, or -1.
    int chosenExercise() const { return (selected >= 0 && selected < (int)shown.size()) ? (int)shown[selected] : -1; }
    std::wstring hostname() const { return hostnameBox->getText(); }
    std::wstring port() const { return portBox->getText(); }

    //Once per frame before drawing: follows the window size, and loads a chart asked for in the last
    //frame (so that "loading" is on screen while it is built).
    void beforeFrame()
    {
        const irr::core::dimension2du size = Environment->getVideoDriver()->getScreenSize();
        if (size != laidOutFor) { layout(size); }
        if (chartPending) {
            chartPending = false;
            loadChart();
        }
        startButton->setEnabled(mode != ModeExercise || chosenExercise() >= 0);
        //Map buttons only over a chart
        const bool mapControls = (mode == ModeExercise) && currentChart() != 0;
        zoomIn->setVisible(mapControls);
        zoomOut->setVisible(mapControls);
        zoomFit->setVisible(mapControls);
    }

    //-----------------------------------------------------------------------------------------------
    //Input
    //-----------------------------------------------------------------------------------------------

    virtual bool onKey(const irr::SEvent::SKeyInput& k)
    {
        if (!k.PressedDown) {
            return k.Key == irr::KEY_RETURN || k.Key == irr::KEY_ESCAPE || k.Key == irr::KEY_UP || k.Key == irr::KEY_DOWN
                || k.Key == irr::KEY_PRIOR || k.Key == irr::KEY_NEXT;
        }
        switch (k.Key) {
        case irr::KEY_ESCAPE:
            device->closeDevice();
            return true;
        case irr::KEY_RETURN:
            start();
            return true;
        case irr::KEY_UP: moveSelection(-1); return true;
        case irr::KEY_DOWN: moveSelection(1); return true;
        case irr::KEY_PRIOR: moveSelection(-visibleRows()); return true;
        case irr::KEY_NEXT: moveSelection(visibleRows()); return true;
        default:
            break;
        }
        //Typing anywhere searches (unless typing in the address or port box).
        if (mode == ModeExercise && k.Char >= 32 && !k.Control) {
            irr::gui::IGUIElement* focus = Environment->getFocus();
            if (focus != search && focus != hostnameBox && focus != portBox) { Environment->setFocus(search); }
        }
        return false;
    }

    virtual bool onMouse(const irr::SEvent::SMouseInput& m)
    {
        const irr::core::vector2df p((irr::f32)m.X, (irr::f32)m.Y);
        mouse = p;
        if (dragging) {
            if (m.Event == irr::EMIE_MOUSE_MOVED) {
                ChartView* chart = currentChart();
                if (chart) { chart->panPixels(m.X - dragFrom.X, m.Y - dragFrom.Y); }
                dragFrom = irr::core::position2di(m.X, m.Y);
                return true;
            }
            if (m.Event == irr::EMIE_LMOUSE_LEFT_UP) { dragging = false; return true; }
        }
        if (mode != ModeExercise) { return false; }
        const bool overControl = overChild(p);

        //List
        hovered = -1;
        if (listArea.isPointInside(p)) {
            const int row = rowAt(p);
            hovered = row;
            if (m.Event == irr::EMIE_MOUSE_WHEEL) {
                scroll -= m.Wheel * rowH * 1.5f;
                clampScroll();
                return true;
            }
            if (m.Event == irr::EMIE_LMOUSE_PRESSED_DOWN && row >= 0) {
                const irr::u32 now = device->getTimer()->getRealTime();
                const bool twice = (row == lastClickRow) && (now - lastClickMs < 450);
                select(row);
                lastClickRow = row;
                lastClickMs = now;
                if (twice) { start(); }
                return true;
            }
            return m.Event == irr::EMIE_LMOUSE_LEFT_UP;
        }

        //Chart: wheel to zoom about the pointer, drag to move, double-click to fit the exercise again
        if (mapArea.isPointInside(p) && !overControl) {
            ChartView* chart = currentChart();
            if (!chart) { return false; }
            if (m.Event == irr::EMIE_MOUSE_WHEEL) {
                chart->zoomAt(irr::core::position2di(m.X, m.Y), m.Wheel > 0 ? 1.0f / 1.3f : 1.3f);
                return true;
            }
            if (m.Event == irr::EMIE_LMOUSE_DOUBLE_CLICK) { fitExercise(); return true; }
            if (m.Event == irr::EMIE_LMOUSE_PRESSED_DOWN) {
                dragging = true;
                dragFrom = irr::core::position2di(m.X, m.Y);
                return true;
            }
        }
        return false;
    }

    virtual bool OnEvent(const irr::SEvent& event)
    {
        if (event.EventType == irr::EET_GUI_EVENT) {
            const irr::s32 id = event.GUIEvent.Caller ? event.GUIEvent.Caller->getID() : -1;
            if (event.GUIEvent.EventType == irr::gui::EGET_BUTTON_CLICKED) {
                switch (id) {
                case ID_START: start(); return true;
                case ID_QUIT: device->closeDevice(); return true;
                case ID_MODE_NORMAL: setMode(ModeExercise); return true;
                case ID_MODE_SECONDARY: setMode(ModeSecondary); return true;
                case ID_MODE_MULTIPLAYER: setMode(ModeMultiplayer); return true;
                case ID_ZOOM_IN: zoomBy(1.0f / 1.5f); return true;
                case ID_ZOOM_OUT: zoomBy(1.5f); return true;
                case ID_ZOOM_FIT: fitExercise(); return true;
                default: break;
                }
            }
            if (event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_CHANGED && id == ID_SEARCH) {
                const int before = chosenExercise();
                filter();
                selected = -1;
                for (size_t i = 0; i < shown.size(); i++) { if ((int)shown[i] == before) { selected = (int)i; } }
                if (selected < 0 && !shown.empty()) { selected = 0; }
                scroll = 0;
                ensureVisible();
                showSelection();
                return true;
            }
            if (event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_ENTER) { start(); return true; }
        }
        return IGUIElement::OnEvent(event);
    }

    //-----------------------------------------------------------------------------------------------
    //Drawing
    //-----------------------------------------------------------------------------------------------

    virtual void draw()
    {
        if (!IsVisible) { return; }
        irr::video::IVideoDriver* driver = Environment->getVideoDriver();
        const irr::f32 W = (irr::f32)AbsoluteRect.getWidth(), H = (irr::f32)AbsoluteRect.getHeight();

        irr::gui::PanelBatch b;
        b.begin(driver);
        //Page, header and footer bands
        b.rectV(irr::core::rect<irr::f32>(0, 0, W, H), irr::video::SColor(255, 12, 24, 42), Ui::backgroundDeep);
        b.rectV(irr::core::rect<irr::f32>(0, 0, W, headerH), irr::video::SColor(255, 17, 33, 58), irr::video::SColor(255, 13, 27, 48));
        b.rect(irr::core::rect<irr::f32>(0, headerH - 1, W, headerH), Ui::rule);
        b.rectV(irr::core::rect<irr::f32>(0, H - footerH, W, H), irr::video::SColor(255, 13, 26, 46), irr::video::SColor(255, 10, 20, 36));
        b.rect(irr::core::rect<irr::f32>(0, H - footerH, W, H - footerH + 1), Ui::rule);
        //Helm mark
        const irr::core::vector2df mark(pad + 24 * s, headerH * 0.5f);
        b.disc(mark, 24 * s, Ui::primaryTop, Ui::primaryBottom);
        drawHelm(b, mark, 14 * s, irr::video::SColor(255, 255, 255, 255));
        //Mode tabs: a track behind them
        irr::core::rect<irr::f32> track = Ui::toF(modeButtons[0]->getAbsolutePosition());
        track.addInternalPoint(Ui::toF(modeButtons[2]->getAbsolutePosition()).LowerRightCorner);
        track.UpperLeftCorner -= irr::core::vector2df(4, 4);
        track.LowerRightCorner += irr::core::vector2df(4, 4);
        Ui::roundRect(b, track, 10, irr::video::SColor(255, 9, 18, 33), irr::video::SColor(255, 9, 18, 33));
        Ui::roundRectOutline(b, track, 10, 1.0f, Ui::edge);
        //Footer field
        if (hostnameBox->isVisible()) { field(b, hostnameBox); }
        b.flush();

        Ui::drawText(fonts.title, modeTitle(), irr::core::rect<irr::f32>(mark.X + 38 * s, headerH * 0.5f - 26 * s, track.UpperLeftCorner.X - 20, headerH * 0.5f + 4 * s), Ui::text);
        Ui::drawText(fonts.small, L"NAUTITECH  \u00B7  Simulateur de Navigation Maritime",
            irr::core::rect<irr::f32>(mark.X + 38 * s, headerH * 0.5f + 4 * s, track.UpperLeftCorner.X - 20, headerH * 0.5f + 26 * s), Ui::textDim);
        if (hostnameBox->isVisible()) {
            const irr::core::rect<irr::f32> f = Ui::toF(hostnameBox->getAbsolutePosition());
            Ui::drawText(fonts.small, french ? L"Envoyer les donn\u00E9es \u00E0 (r\u00E9p\u00E9titeur, affichages secondaires, contr\u00F4leur)"
                : L"Send the data to (repeater, secondary displays, controller)",
                irr::core::rect<irr::f32>(f.UpperLeftCorner.X - 10 * s, f.UpperLeftCorner.Y - 24 * s, startArea.UpperLeftCorner.X - 16, f.UpperLeftCorner.Y - 4 * s), Ui::textDim);
        }

        if (mode == ModeExercise) { drawExercise(driver); }
        else { drawStationMode(driver); }

        IGUIElement::draw(); //edit boxes and buttons

        //Placeholders in empty boxes
        if (search->isVisible() && std::wstring(search->getText()).empty()) {
            const irr::core::rect<irr::f32> f = Ui::toF(search->getAbsolutePosition());
            const irr::core::rect<irr::s32> clip = Ui::toI(f);
            Ui::drawText(fonts.text, french ? L"Rechercher un exercice ou une zone" : L"Search an exercise or an area", f, Ui::textFaint, Ui::Left, &clip);
        }
        if (hostnameBox->isVisible() && std::wstring(hostnameBox->getText()).empty()) {
            Ui::drawText(fonts.text, french ? L"Aucun (ce poste seul)" : L"None (this station only)", Ui::toF(hostnameBox->getAbsolutePosition()), Ui::textFaint);
        }
    }

private:
    //-----------------------------------------------------------------------------------------------
    //Layout
    //-----------------------------------------------------------------------------------------------

    void layout(const irr::core::dimension2du& size)
    {
        laidOutFor = size;
        setRelativePosition(irr::core::rect<irr::s32>(0, 0, (irr::s32)size.Width, (irr::s32)size.Height));
        const irr::f32 W = (irr::f32)size.Width, H = (irr::f32)size.Height;
        s = irr::core::clamp(std::min(W / 1600.0f, H / 980.0f), 0.72f, 2.0f);
        loadFonts();

        pad = 24 * s;
        gap = 16 * s;
        headerH = 84 * s;
        footerH = 92 * s;
        rowH = 70 * s;

        //Header: mode tabs on the right
        irr::f32 x = W - pad - 4;
        const irr::f32 tabH = 38 * s, tabY = headerH * 0.5f - tabH * 0.5f;
        for (int i = 2; i >= 0; i--) {
            const irr::f32 w = Ui::textWidth(fonts.text, modeButtons[i]->getText()) + 34 * s;
            modeButtons[i]->setRelativePosition(Ui::toI(irr::core::rect<irr::f32>(x - w, tabY, x, tabY + tabH)));
            modeButtons[i]->setFont(fonts.text);
            x -= w + 4;
        }

        //Footer: send-to field on the left, buttons on the right
        const irr::f32 buttonH = 48 * s, fy = H - footerH * 0.5f + 10 * s;
        const irr::f32 startW = std::max(260 * s, Ui::textWidth(fonts.name, startLabel()) + 60 * s), quitW = 130 * s;
        startArea = irr::core::rect<irr::f32>(W - pad - startW - gap - quitW, fy - buttonH * 0.5f, W - pad, fy + buttonH * 0.5f);
        startButton->setRelativePosition(Ui::toI(irr::core::rect<irr::f32>(W - pad - startW, fy - buttonH * 0.5f, W - pad, fy + buttonH * 0.5f)));
        startButton->setFont(fonts.name);
        quitButton->setRelativePosition(Ui::toI(irr::core::rect<irr::f32>(W - pad - startW - gap - quitW, fy - buttonH * 0.5f, W - pad - startW - gap, fy + buttonH * 0.5f)));
        quitButton->setFont(fonts.text);
        const irr::f32 fieldW = std::min(560 * s, startArea.UpperLeftCorner.X - pad * 2 - 20 * s);
        hostnameBox->setRelativePosition(Ui::toI(irr::core::rect<irr::f32>(pad + 10 * s, fy - 16 * s, pad + 10 * s + fieldW - 20 * s, fy + 18 * s)));
        hostnameBox->setOverrideFont(fonts.text);

        //Body
        body = irr::core::rect<irr::f32>(pad, headerH + gap, W - pad, H - footerH - gap);
        const irr::f32 listW = irr::core::clamp(W * 0.27f, 300 * s, 440 * s);
        listCard = irr::core::rect<irr::f32>(body.UpperLeftCorner.X, body.UpperLeftCorner.Y, body.UpperLeftCorner.X + listW, body.LowerRightCorner.Y);
        searchField = irr::core::rect<irr::f32>(listCard.UpperLeftCorner.X + 14 * s, listCard.UpperLeftCorner.Y + 14 * s,
            listCard.LowerRightCorner.X - 14 * s, listCard.UpperLeftCorner.Y + 14 * s + 40 * s);
        search->setRelativePosition(Ui::toI(irr::core::rect<irr::f32>(searchField.UpperLeftCorner.X + 36 * s, searchField.UpperLeftCorner.Y,
            searchField.LowerRightCorner.X - 10 * s, searchField.LowerRightCorner.Y)));
        search->setOverrideFont(fonts.text);
        listArea = irr::core::rect<irr::f32>(listCard.UpperLeftCorner.X + 8 * s, searchField.LowerRightCorner.Y + 36 * s,
            listCard.LowerRightCorner.X - 8 * s, listCard.LowerRightCorner.Y - 8 * s);

        //Quick view: title, chart, description; facts in a column on the right (or a row when narrow)
        const irr::core::rect<irr::f32> right(listCard.LowerRightCorner.X + gap, body.UpperLeftCorner.Y, body.LowerRightCorner.X, body.LowerRightCorner.Y);
        wide = right.getWidth() >= 860 * s;
        irr::f32 mapRight = right.LowerRightCorner.X;
        if (wide) {
            const irr::f32 infoW = irr::core::clamp(right.getWidth() * 0.3f, 280 * s, 380 * s);
            infoArea = irr::core::rect<irr::f32>(right.LowerRightCorner.X - infoW, right.UpperLeftCorner.Y, right.LowerRightCorner.X, right.LowerRightCorner.Y);
            mapRight = infoArea.UpperLeftCorner.X - gap;
        }
        titleArea = irr::core::rect<irr::f32>(right.UpperLeftCorner.X, right.UpperLeftCorner.Y, mapRight, right.UpperLeftCorner.Y + 70 * s);
        const irr::f32 descH = wide ? irr::core::clamp(right.getHeight() * 0.22f, 120 * s, 220 * s) : irr::core::clamp(right.getHeight() * 0.15f, 96 * s, 160 * s);
        descCard = irr::core::rect<irr::f32>(right.UpperLeftCorner.X, right.LowerRightCorner.Y - descH, mapRight, right.LowerRightCorner.Y);
        irr::f32 mapBottom = descCard.UpperLeftCorner.Y - gap;
        if (!wide) {
            const irr::f32 lh = Ui::textHeight(fonts.small) + 7 * s;
            const irr::f32 infoH = 56 * s + (lh + 4 * s) + 6 * lh; //a headline and six lines
            infoArea = irr::core::rect<irr::f32>(right.UpperLeftCorner.X, mapBottom - infoH, mapRight, mapBottom);
            mapBottom = infoArea.UpperLeftCorner.Y - gap;
        }
        mapCard = irr::core::rect<irr::f32>(right.UpperLeftCorner.X, titleArea.LowerRightCorner.Y, mapRight, mapBottom);
        mapArea = irr::core::rect<irr::f32>(mapCard.UpperLeftCorner.X + 8 * s, mapCard.UpperLeftCorner.Y + 8 * s,
            mapCard.LowerRightCorner.X - 8 * s, mapCard.LowerRightCorner.Y - 8 * s);
        const irr::f32 zb = 36 * s, zx = mapArea.LowerRightCorner.X - 10 * s, zy = mapArea.UpperLeftCorner.Y + 10 * s;
        const irr::f32 fitW = Ui::textWidth(fonts.small, zoomFit->getText()) + 26 * s;
        zoomFit->setRelativePosition(Ui::toI(irr::core::rect<irr::f32>(zx - fitW, zy, zx, zy + zb)));
        zoomOut->setRelativePosition(Ui::toI(irr::core::rect<irr::f32>(zx - fitW - 6 * s - zb, zy, zx - fitW - 6 * s, zy + zb)));
        zoomIn->setRelativePosition(Ui::toI(irr::core::rect<irr::f32>(zx - fitW - 12 * s - 2 * zb, zy, zx - fitW - 12 * s - zb, zy + zb)));
        zoomFit->setFont(fonts.small);
        for (std::map<std::string, ChartView*>::iterator it = charts.begin(); it != charts.end(); ++it) {
            if (it->second) { it->second->setViewport(Ui::toI(mapArea)); }
        }

        //Secondary / multiplayer: one card in the middle, with the port
        //(as tall as its text, the port field under the text)
        const irr::f32 cardW = std::min(760 * s, W - 2 * pad);
        stationCard = irr::core::rect<irr::f32>(W * 0.5f - cardW * 0.5f, body.UpperLeftCorner.Y + 40 * s, W * 0.5f + cardW * 0.5f, 0);
        const irr::f32 textX = stationCard.UpperLeftCorner.X + 112 * s;
        const size_t textLines = Ui::wrap(fonts.text, stationText(), stationCard.LowerRightCorner.X - 36 * s - textX).size();
        const irr::f32 textBottom = stationCard.UpperLeftCorner.Y + 92 * s + textLines * (Ui::textHeight(fonts.text) + 4 * s);
        portField = irr::core::rect<irr::f32>(textX, textBottom + 54 * s, textX + 200 * s, textBottom + 94 * s);
        stationCard.LowerRightCorner.Y = portField.LowerRightCorner.Y + 40 * s;
        portBox->setRelativePosition(Ui::toI(irr::core::rect<irr::f32>(portField.UpperLeftCorner.X + 12 * s, portField.UpperLeftCorner.Y,
            portField.LowerRightCorner.X - 8 * s, portField.LowerRightCorner.Y)));
        portBox->setOverrideFont(fonts.name);

        clampScroll();
        if (mapFor >= 0) { fitExercise(); }
        else { showSelection(); }
    }

    void loadFonts()
    {
        irr::gui::IGUIFont* fallback = Environment->getSkin()->getFont();
        auto load = [&](int size) -> irr::gui::IGUIFont* {
            size = irr::core::clamp((int)(size * s + 0.5f), 12, 36);
            const std::string path = "media/fonts/" + fontName + "/" + fontName + "-" + std::to_string(size) + ".xml";
            irr::gui::IGUIFont* f = fontName.empty() ? 0 : Environment->getFont(path.c_str());
            if (!f) { f = fallback; }
            Ui::cleanFontAtlas(f);
            return f;
        };
        fonts.title = load(26);
        fonts.heading = load(24);
        fonts.name = load(17);
        fonts.text = load(15);
        fonts.small = load(13);
        fonts.tiny = load(12);
    }

    //-----------------------------------------------------------------------------------------------
    //List and selection
    //-----------------------------------------------------------------------------------------------

    void filter()
    {
        shown.clear();
        const std::wstring q = lower(trimText(search ? std::wstring(search->getText()) : std::wstring()));
        for (size_t i = 0; i < exercises.size(); i++) {
            if (q.empty() || exercises[i].searchText.find(q) != std::wstring::npos) { shown.push_back(i); }
        }
    }

    int visibleRows() const { return std::max(1, (int)(listArea.getHeight() / rowH)); }

    int rowAt(const irr::core::vector2df& p) const
    {
        const int row = (int)std::floor((p.Y - listArea.UpperLeftCorner.Y + scroll) / rowH);
        return (row >= 0 && row < (int)shown.size()) ? row : -1;
    }

    void clampScroll()
    {
        const irr::f32 maxScroll = std::max(0.0f, shown.size() * rowH - listArea.getHeight());
        scroll = irr::core::clamp(scroll, 0.0f, maxScroll);
    }

    void ensureVisible()
    {
        if (selected < 0) { return; }
        const irr::f32 top = selected * rowH, bottom = top + rowH;
        if (top < scroll) { scroll = top; }
        if (bottom > scroll + listArea.getHeight()) { scroll = bottom - listArea.getHeight(); }
        clampScroll();
    }

    void moveSelection(int by)
    {
        if (mode != ModeExercise || shown.empty()) { return; }
        select(irr::core::clamp(selected + by, 0, (int)shown.size() - 1));
    }

    void select(int row)
    {
        if (row == selected) { return; }
        selected = row;
        ensureVisible();
        showSelection();
    }

    //The chart of the selected exercise's area, fitted on its ships (loaded at the next frame if needed).
    void showSelection()
    {
        const int e = chosenExercise();
        mapFor = e;
        if (e < 0) { return; }
        const std::string world = exercises[e].data.worldName;
        if (charts.find(world) == charts.end()) {
            chartPending = true;
            return;
        }
        fitExercise();
    }

    void loadChart()
    {
        const int e = chosenExercise();
        if (e < 0) { return; }
        const std::string world = exercises[e].data.worldName;
        if (charts.find(world) != charts.end()) { fitExercise(); return; }
        ChartView* chart = new ChartView();
        chart->setPreviewStyle(ChartView::Style_Day);
        std::string error;
        if (world.empty() || !chart->load(device, world, error)) {
            delete chart;
            chart = 0;
            chartErrors[world] = decodeText(error);
        }
        charts[world] = chart;
        if (chart) { chart->setViewport(Ui::toI(mapArea)); }
        fitExercise();
    }

    ChartView* currentChart() const
    {
        if (mapFor < 0) { return 0; }
        std::map<std::string, ChartView*>::const_iterator it = charts.find(exercises[mapFor].data.worldName);
        return (it == charts.end()) ? 0 : it->second;
    }

    //Every point of the exercise in chart metres: own ship, the other ships and their routes, the incident.
    void exercisePoints(const ChartView& chart, std::vector<irr::core::vector2df>& points) const
    {
        const Exercise& e = exercises[mapFor];
        points.push_back(irr::core::vector2df((irr::f32)chart.lonToX(e.data.ownShipData.initialLong), (irr::f32)chart.latToZ(e.data.ownShipData.initialLat)));
        for (size_t i = 0; i < e.data.otherShipsData.size(); i++) {
            std::vector<irr::core::vector2df> route;
            routeOf(chart, e.data.otherShipsData[i], route);
            points.insert(points.end(), route.begin(), route.end());
        }
        if (e.hasIncident) {
            for (size_t i = 0; i < e.incident.survivors.size(); i++) {
                const IncidentPoint& q = e.incident.survivors[i].pos;
                if (q.lat != 0 || q.lon != 0) { points.push_back(irr::core::vector2df((irr::f32)chart.lonToX(q.lon), (irr::f32)chart.latToZ(q.lat))); }
            }
        }
    }

    //Start position, then the end of each leg, in chart metres.
    static void routeOf(const ChartView& chart, const OtherShipData& ship, std::vector<irr::core::vector2df>& route)
    {
        double x = chart.lonToX(ship.initialLong), z = chart.latToZ(ship.initialLat);
        route.push_back(irr::core::vector2df((irr::f32)x, (irr::f32)z));
        for (size_t l = 0; l < ship.legs.size(); l++) {
            const double d = ship.legs[l].distance * 1852.0;
            if (d <= 0) { continue; }
            x += std::sin(ship.legs[l].bearing * kPi / 180.0) * d;
            z += std::cos(ship.legs[l].bearing * kPi / 180.0) * d;
            route.push_back(irr::core::vector2df((irr::f32)x, (irr::f32)z));
        }
    }

    void fitExercise()
    {
        ChartView* chart = currentChart();
        if (!chart) { return; }
        chart->setViewport(Ui::toI(mapArea));
        std::vector<irr::core::vector2df> points;
        exercisePoints(*chart, points);
        irr::core::rect<irr::f32> box(points[0], points[0]);
        for (size_t i = 1; i < points.size(); i++) { box.addInternalPoint(points[i]); }
        //At least 1.5 NM across, so that a harbour exercise still shows its surroundings.
        const irr::f32 span = std::max(std::max(box.getWidth(), box.getHeight()), 2800.0f);
        chart->centreOnXZ(box.getCenter().X, box.getCenter().Y);
        const irr::f32 w = std::max(1.0f, mapArea.getWidth()), h = std::max(1.0f, mapArea.getHeight());
        chart->setMetresPerPixel(std::max(std::max(box.getWidth(), span * 0.6f) / w, std::max(box.getHeight(), span * 0.6f) / h) * 1.6f);
    }

    void zoomBy(irr::f32 factor)
    {
        ChartView* chart = currentChart();
        if (chart) { chart->zoomAt(Ui::toI(mapArea).getCenter(), factor); }
    }

    bool overChild(const irr::core::vector2df& p) const
    {
        for (irr::core::list<irr::gui::IGUIElement*>::ConstIterator it = Children.begin(); it != Children.end(); ++it) {
            if ((*it)->isVisible() && Ui::toF((*it)->getAbsolutePosition()).isPointInside(p)) { return true; }
        }
        return false;
    }

    //-----------------------------------------------------------------------------------------------
    //Mode and start
    //-----------------------------------------------------------------------------------------------

    void setMode(Mode m)
    {
        mode = m;
        for (int i = 0; i < 3; i++) { modeButtons[i]->setChecked(i == (int)m); }
        const bool exercise = (m == ModeExercise);
        search->setVisible(exercise);
        portBox->setVisible(!exercise);
        hostnameBox->setVisible(m != ModeSecondary);
        startButton->setText(startLabel().c_str());
        laidOutFor = irr::core::dimension2du(0, 0); //lay out again (the start button follows its label)
        Environment->setFocus(exercise ? (irr::gui::IGUIElement*)search : (irr::gui::IGUIElement*)portBox);
        if (!exercise) {
            //Caret after the port number
            irr::SEvent end;
            end.EventType = irr::EET_KEY_INPUT_EVENT;
            end.KeyInput.Key = irr::KEY_END;
            end.KeyInput.Char = 0;
            end.KeyInput.PressedDown = true;
            end.KeyInput.Shift = false;
            end.KeyInput.Control = false;
            end.KeyInput.AutoRepeat = false;
            end.KeyInput.Extended = false;
            portBox->OnEvent(end);
        }
    }

    std::wstring startLabel() const
    {
        if (mode == ModeSecondary) { return french ? L"D\u00E9marrer l'affichage" : L"Start the display"; }
        if (mode == ModeMultiplayer) { return french ? L"Rejoindre l'exercice" : L"Join the exercise"; }
        return french ? L"Lancer l'exercice" : L"Start the exercise";
    }

    std::wstring modeTitle() const
    {
        if (mode == ModeSecondary) { return french ? L"Affichage secondaire" : L"Secondary display"; }
        if (mode == ModeMultiplayer) { return french ? L"Exercice multijoueur" : L"Multiplayer exercise"; }
        return french ? L"Choisir un exercice" : L"Choose an exercise";
    }

    void start()
    {
        if (mode == ModeExercise && chosenExercise() < 0) { return; }
        started = true;
    }

    //-----------------------------------------------------------------------------------------------
    //Drawing: exercise mode
    //-----------------------------------------------------------------------------------------------

    void drawExercise(irr::video::IVideoDriver* driver)
    {
        irr::gui::PanelBatch b;
        b.begin(driver);
        Ui::card(b, listCard, 14);
        field(b, search);
        drawSearchIcon(b, irr::core::vector2df(searchField.UpperLeftCorner.X + 18 * s, searchField.getCenter().Y), 7 * s);
        //Rows
        const irr::core::rect<irr::s32> listClip = Ui::toI(listArea);
        for (size_t i = 0; i < shown.size(); i++) {
            const irr::f32 y = listArea.UpperLeftCorner.Y + i * rowH - scroll;
            if (y + rowH < listArea.UpperLeftCorner.Y || y > listArea.LowerRightCorner.Y) { continue; }
            irr::core::rect<irr::f32> r(listArea.UpperLeftCorner.X, y + 2 * s, listArea.LowerRightCorner.X - 8 * s, y + rowH - 3 * s);
            r.clipAgainst(listArea);
            if (r.getHeight() <= 0) { continue; }
            if ((int)i == selected) {
                Ui::roundRect(b, r, 10, Ui::primaryTop, Ui::primaryBottom);
                Ui::roundRectOutline(b, r, 10, 1.0f, irr::video::SColor(160, 190, 225, 255));
            }
            else if ((int)i == hovered) {
                Ui::roundRect(b, r, 10, irr::video::SColor(255, 34, 56, 88), irr::video::SColor(255, 28, 48, 76));
            }
        }
        //Scroll bar
        const irr::f32 total = shown.size() * rowH;
        if (total > listArea.getHeight()) {
            const irr::f32 trackH = listArea.getHeight();
            const irr::f32 thumbH = std::max(30 * s, trackH * trackH / total);
            const irr::f32 thumbY = listArea.UpperLeftCorner.Y + (trackH - thumbH) * (scroll / (total - trackH));
            const irr::f32 x = listArea.LowerRightCorner.X - 3 * s;
            Ui::roundRect(b, irr::core::rect<irr::f32>(x - 2 * s, thumbY, x + 2 * s, thumbY + thumbH), 2 * s, irr::video::SColor(140, 120, 160, 210), irr::video::SColor(140, 120, 160, 210));
        }
        //Badges on the rows
        for (size_t i = 0; i < shown.size(); i++) {
            const irr::f32 y = listArea.UpperLeftCorner.Y + i * rowH - scroll;
            if (y < listArea.UpperLeftCorner.Y - 1 || y + rowH > listArea.LowerRightCorner.Y + 1) { continue; }
            const Exercise& e = exercises[shown[i]];
            irr::f32 bx = listArea.LowerRightCorner.X - 20 * s;
            const irr::f32 by = y + rowH * 0.5f + 4 * s, bh = 20 * s;
            if (e.hasIncident && !e.incident.survivors.empty()) {
                const std::wstring t = L"SAR";
                bx -= Ui::textWidth(fonts.tiny, t) + bh * 0.9f;
                pill(b, fonts.tiny, t, bx, by, bh, irr::video::SColor(255, 18, 92, 96), irr::video::SColor(200, 110, 220, 210));
                badges.push_back(Badge(t, irr::core::rect<irr::f32>(bx, by, bx + Ui::textWidth(fonts.tiny, t) + bh * 0.9f, by + bh)));
                bx -= 6 * s;
            }
            if (e.hasIncident) {
                const std::wstring t = french ? L"Incendie" : L"Fire";
                bx -= Ui::textWidth(fonts.tiny, t) + bh * 0.9f;
                pill(b, fonts.tiny, t, bx, by, bh, irr::video::SColor(255, 110, 42, 22), irr::video::SColor(200, 255, 150, 90));
                badges.push_back(Badge(t, irr::core::rect<irr::f32>(bx, by, bx + Ui::textWidth(fonts.tiny, t) + bh * 0.9f, by + bh)));
            }
        }
        //Quick view frames
        Ui::card(b, mapCard, 14);
        Ui::card(b, descCard, 14);
        b.flush();

        //List texts
        const std::wstring count = std::to_wstring(shown.size()) + (french ? (shown.size() > 1 ? L" exercices" : L" exercice") : (shown.size() == 1 ? L" exercise" : L" exercises"));
        Ui::drawText(fonts.small, count, irr::core::rect<irr::f32>(listArea.UpperLeftCorner.X + 6 * s, searchField.LowerRightCorner.Y + 8 * s,
            listArea.LowerRightCorner.X, searchField.LowerRightCorner.Y + 30 * s), Ui::textFaint);
        for (size_t i = 0; i < shown.size(); i++) {
            const irr::f32 y = listArea.UpperLeftCorner.Y + i * rowH - scroll;
            if (y + rowH < listArea.UpperLeftCorner.Y || y > listArea.LowerRightCorner.Y) { continue; }
            const Exercise& e = exercises[shown[i]];
            const bool sel = ((int)i == selected);
            const irr::core::rect<irr::s32> nameClip(listClip.UpperLeftCorner.X, listClip.UpperLeftCorner.Y, listClip.LowerRightCorner.X - (irr::s32)(18 * s), listClip.LowerRightCorner.Y);
            Ui::drawText(fonts.name, e.name, irr::core::rect<irr::f32>(listArea.UpperLeftCorner.X + 14 * s, y + 8 * s, listArea.LowerRightCorner.X - 18 * s, y + rowH * 0.5f),
                sel ? irr::video::SColor(255, 255, 255, 255) : Ui::text, Ui::Left, &nameClip);
            irr::f32 subRight = listArea.LowerRightCorner.X - 18 * s;
            for (size_t k = 0; k < badges.size(); k++) {
                if (badges[k].area.UpperLeftCorner.Y >= y && badges[k].area.UpperLeftCorner.Y < y + rowH) { subRight = std::min(subRight, badges[k].area.UpperLeftCorner.X - 6 * s); }
            }
            //Area, start time, ships: the ones that fit beside the badges
            std::wstring sub = decodeText(e.data.worldName);
            const size_t n = e.data.otherShipsData.size();
            std::vector<std::wstring> more;
            more.push_back(clockTime(e.data.startTime));
            if (n > 0) { more.push_back(countOf(n, french ? L"navire" : L"ship", french ? L"navires" : L"ships")); }
            for (size_t k = 0; k < more.size(); k++) {
                const std::wstring longer = sub + L"  \u00B7  " + more[k];
                if (listArea.UpperLeftCorner.X + 14 * s + Ui::textWidth(fonts.small, longer) > subRight) { break; }
                sub = longer;
            }
            const irr::core::rect<irr::s32> subClip(listClip.UpperLeftCorner.X, listClip.UpperLeftCorner.Y, (irr::s32)subRight, listClip.LowerRightCorner.Y);
            Ui::drawText(fonts.small, sub, irr::core::rect<irr::f32>(listArea.UpperLeftCorner.X + 14 * s, y + rowH * 0.5f, subRight, y + rowH - 10 * s),
                sel ? irr::video::SColor(255, 214, 230, 248) : Ui::textDim, Ui::Left, &subClip);
        }
        for (size_t k = 0; k < badges.size(); k++) {
            Ui::drawText(fonts.tiny, badges[k].text, badges[k].area, irr::video::SColor(255, 255, 255, 255), Ui::Centre, &listClip);
        }
        badges.clear();
        if (exercises.empty() || shown.empty()) {
            const std::wstring t = exercises.empty()
                ? (french ? L"Aucun exercice dans le dossier Scenarios. Cr\u00E9ez-en un avec l'\u00E9diteur de sc\u00E9nario." : L"No exercise in the Scenarios folder. Create one with the scenario editor.")
                : (french ? L"Aucun exercice ne correspond \u00E0 la recherche." : L"No exercise matches the search.");
            Ui::drawWrapped(fonts.text, t, irr::core::rect<irr::f32>(listArea.UpperLeftCorner.X + 14 * s, listArea.UpperLeftCorner.Y + 20 * s,
                listArea.LowerRightCorner.X - 14 * s, listArea.LowerRightCorner.Y), Ui::textDim);
        }

        const int e = chosenExercise();
        if (e < 0) {
            const std::wstring t = french ? L"Choisissez un exercice dans la liste pour en voir l'aper\u00E7u." : L"Choose an exercise in the list to see its overview.";
            Ui::drawText(fonts.text, t, mapCard, Ui::textFaint, Ui::Centre);
            return;
        }
        drawTitle(exercises[e]);
        drawMap(driver, exercises[e]);
        drawFacts(driver, exercises[e]);
        drawDescription(exercises[e]);
    }

    void drawTitle(const Exercise& e)
    {
        const irr::core::rect<irr::s32> clip = Ui::toI(titleArea);
        Ui::drawText(fonts.heading, e.name, irr::core::rect<irr::f32>(titleArea.UpperLeftCorner.X + 4 * s, titleArea.UpperLeftCorner.Y,
            titleArea.LowerRightCorner.X, titleArea.UpperLeftCorner.Y + 36 * s), Ui::text, Ui::Left, &clip);
        //Area, date, time of day
        std::vector<std::wstring> chips;
        chips.push_back(decodeText(e.data.worldName));
        chips.push_back(dateText(e.data.startDay, e.data.startMonth, e.data.startYear, french));
        chips.push_back(clockTime(e.data.startTime) + L"  \u00B7  " + dayPeriod(e.data));
        irr::gui::PanelBatch b;
        b.begin(Environment->getVideoDriver());
        irr::f32 x = titleArea.UpperLeftCorner.X + 4 * s;
        const irr::f32 y = titleArea.UpperLeftCorner.Y + 40 * s, h = 24 * s;
        std::vector<irr::core::rect<irr::f32> > areas;
        for (size_t i = 0; i < chips.size(); i++) {
            const irr::f32 w = pill(b, fonts.small, chips[i], x, y, h, irr::video::SColor(255, 20, 38, 64), irr::video::SColor(120, 110, 160, 220));
            areas.push_back(irr::core::rect<irr::f32>(x, y, x + w, y + h));
            x += w + 8 * s;
        }
        b.flush();
        for (size_t i = 0; i < chips.size(); i++) { Ui::drawText(fonts.small, chips[i], areas[i], Ui::textDim, Ui::Centre, &clip); }
    }

    std::wstring dayPeriod(const ScenarioData& d) const
    {
        const irr::f32 t = d.startTime, rise = d.sunRise > 0 ? d.sunRise : 6.0f, set = d.sunSet > 0 ? d.sunSet : 18.0f;
        if (std::fabs(t - rise) < 0.75f) { return french ? L"aube" : L"dawn"; }
        if (std::fabs(t - set) < 0.75f) { return french ? L"cr\u00E9puscule" : L"dusk"; }
        if (t > rise && t < set) { return french ? L"jour" : L"day"; }
        return french ? L"nuit" : L"night";
    }

    void drawMap(irr::video::IVideoDriver* driver, const Exercise& e)
    {
        ChartView* chart = currentChart();
        const irr::core::recti viewport = Ui::toI(mapArea);
        if (!chart) {
            driver->draw2DRectangle(irr::video::SColor(255, 10, 20, 36), viewport);
            std::wstring message;
            const bool pending = charts.find(e.data.worldName) == charts.end();
            if (pending) { message = french ? L"Chargement de la carte\u2026" : L"Loading the chart\u2026"; }
            else {
                message = (french ? L"Carte indisponible pour la zone \u00AB " : L"No chart for the area \u00AB ") + decodeText(e.data.worldName) + L" \u00BB";
                std::map<std::string, std::wstring>::const_iterator it = chartErrors.find(e.data.worldName);
                if (it != chartErrors.end() && !it->second.empty()) { message += L"\n" + it->second; }
            }
            Ui::drawWrapped(fonts.text, message, irr::core::rect<irr::f32>(mapArea.UpperLeftCorner.X + 30 * s, mapArea.getCenter().Y - 20 * s,
                mapArea.LowerRightCorner.X - 30 * s, mapArea.LowerRightCorner.Y), Ui::textDim);
            return;
        }
        chart->setViewport(viewport);
        chart->draw(driver);
        chart->drawGraticule(driver, fonts.tiny, (irr::s32)(56 * s), (irr::s32)(legendHeight(e) + 8 * s));
        const bool light = chart->lightBackground();
        const irr::core::rect<irr::f32> view = mapArea;
        auto toScreen = [&](const irr::core::vector2df& m) {
            const irr::core::position2di p = chart->toScreenXZ(m.X, m.Y);
            return irr::core::vector2df((irr::f32)p.X, (irr::f32)p.Y);
        };
        auto inside = [&](const irr::core::vector2df& p, irr::f32 margin) {
            return p.X >= view.UpperLeftCorner.X + margin && p.X <= view.LowerRightCorner.X - margin && p.Y >= view.UpperLeftCorner.Y + margin && p.Y <= view.LowerRightCorner.Y - margin;
        };

        //Routes of the other ships, then the ships
        const irr::video::SColor routeCol = light ? irr::video::SColor(230, 196, 96, 10) : irr::video::SColor(230, 255, 180, 70);
        irr::gui::PanelBatch b;
        b.begin(driver);
        std::vector<std::vector<irr::core::vector2df> > routes(e.data.otherShipsData.size());
        for (size_t i = 0; i < e.data.otherShipsData.size(); i++) {
            routeOf(*chart, e.data.otherShipsData[i], routes[i]);
            for (size_t k = 0; k + 1 < routes[i].size(); k++) {
                irr::core::vector2df a = toScreen(routes[i][k]), c = toScreen(routes[i][k + 1]);
                //Dashed: 9 px on, 6 off
                const irr::f32 len = (c - a).getLength();
                const irr::core::vector2df dir = len > 0 ? (c - a) / len : irr::core::vector2df(0, 0);
                for (irr::f32 t = 0; t < len; t += 15 * s) {
                    irr::core::vector2df p0 = a + dir * t, p1 = a + dir * std::min(len, t + 9 * s);
                    if (clipSegment(p0, p1, view)) { b.line(p0, p1, 2.0f * s, routeCol); }
                }
                if (inside(c, 3)) { b.disc(c, 3.5f * s, routeCol, routeCol); }
            }
        }
        //Own ship's speed vector: six minutes ahead
        const OwnShipData& own = e.data.ownShipData;
        const irr::core::vector2df ownM((irr::f32)chart->lonToX(own.initialLong), (irr::f32)chart->latToZ(own.initialLat));
        const irr::core::vector2df ownP = toScreen(ownM);
        if (own.initialSpeed > 0) {
            const irr::f32 d = own.initialSpeed * 1852.0f * 0.1f;
            irr::core::vector2df tip = toScreen(ownM + irr::core::vector2df(std::sin(own.initialBearing * (irr::f32)kPi / 180.0f) * d, std::cos(own.initialBearing * (irr::f32)kPi / 180.0f) * d));
            irr::core::vector2df from = ownP;
            if (clipSegment(from, tip, view)) { b.line(from, tip, 2.0f * s, irr::video::SColor(255, 64, 156, 240)); }
        }
        //Incident: survivors in the water, helicopter base and landing pad
        if (e.hasIncident) {
            for (size_t i = 0; i < e.incident.survivors.size(); i++) {
                const IncidentPoint& q = e.incident.survivors[i].pos;
                if (q.lat == 0 && q.lon == 0) { continue; }
                const irr::core::vector2df p = toScreen(irr::core::vector2df((irr::f32)chart->lonToX(q.lon), (irr::f32)chart->latToZ(q.lat)));
                if (!inside(p, 6)) { continue; }
                b.disc(p, 5 * s, irr::video::SColor(255, 255, 255, 255), irr::video::SColor(255, 255, 255, 255));
                b.disc(p, 3.5f * s, irr::video::SColor(255, 226, 60, 60), irr::video::SColor(255, 226, 60, 60));
            }
        }
        b.flush();

        for (size_t i = 0; i < e.data.otherShipsData.size(); i++) {
            const OtherShipData& ship = e.data.otherShipsData[i];
            const irr::core::vector2df p = toScreen(routes[i][0]);
            if (!inside(p, 10 * s)) { continue; }
            const irr::f32 heading = ship.legs.empty() ? 0.0f : ship.legs[0].bearing;
            irr::video::SColor fill(255, 255, 170, 60);
            if (e.hasIncident && e.incident.casualtyShip == (int)i + 1) { fill = irr::video::SColor(255, 230, 60, 50); }
            for (size_t k = 0; k < e.incident.sarBoats.size() && e.hasIncident; k++) {
                if (e.incident.sarBoats[k].ship == (int)i + 1) { fill = irr::video::SColor(255, 40, 200, 190); }
            }
            ChartDraw::ship(driver, irr::core::position2di((irr::s32)p.X, (irr::s32)p.Y), heading, fill, irr::video::SColor(255, 30, 30, 30), 9 * s);
        }
        if (inside(ownP, 12 * s)) {
            ChartDraw::ship(driver, irr::core::position2di((irr::s32)ownP.X, (irr::s32)ownP.Y), own.initialBearing, irr::video::SColor(255, 40, 130, 235),
                irr::video::SColor(255, 255, 255, 255), 12 * s);
        }

        //Labels (when the chart is not crowded)
        const irr::core::recti clip = viewport;
        if (e.data.otherShipsData.size() <= 12) {
            for (size_t i = 0; i < e.data.otherShipsData.size(); i++) {
                const irr::core::vector2df p = toScreen(routes[i][0]);
                if (!inside(p, 10 * s)) { continue; }
                ChartDraw::text(fonts.tiny, shipLabel(e.data.otherShipsData[i].shipName), irr::core::position2di((irr::s32)(p.X + 12 * s), (irr::s32)(p.Y - 8 * s)),
                    light ? irr::video::SColor(255, 90, 50, 0) : irr::video::SColor(255, 255, 210, 150), &clip, false, light);
            }
        }
        if (inside(ownP, 12 * s)) {
            ChartDraw::text(fonts.small, shipLabel(own.ownShipName), irr::core::position2di((irr::s32)(ownP.X + 15 * s), (irr::s32)(ownP.Y + 2 * s)),
                light ? irr::video::SColor(255, 10, 60, 140) : irr::video::SColor(255, 160, 210, 255), &clip, false, light);
        }
        ChartDraw::scaleBar(driver, fonts.tiny, *chart, (irr::s32)(8 * s));
        drawLegend(driver, e, light);
    }

    //Height of the legend in the chart's lower left corner (no latitude labels behind it).
    irr::f32 legendHeight(const Exercise& e) const
    {
        size_t rows = 1;
        if (!e.data.otherShipsData.empty()) { rows++; }
        if (e.hasIncident) {
            rows++;
            if (!e.incident.sarBoats.empty()) { rows++; }
            if (!e.incident.survivors.empty()) { rows++; }
        }
        return (Ui::textHeight(fonts.tiny) + 6 * s) * rows + 14 * s;
    }

    void drawLegend(irr::video::IVideoDriver* driver, const Exercise& e, bool light)
    {
        struct Item { std::wstring text; irr::video::SColor col; int kind; };
        std::vector<Item> items;
        Item own = { french ? L"Navire de l'exercice" : L"Own ship", irr::video::SColor(255, 40, 130, 235), 0 };
        items.push_back(own);
        if (!e.data.otherShipsData.empty()) {
            Item other = { french ? L"Autres navires et routes" : L"Other ships and routes", irr::video::SColor(255, 255, 170, 60), 0 };
            items.push_back(other);
        }
        if (e.hasIncident) {
            Item fire = { french ? L"Navire en feu" : L"Ship on fire", irr::video::SColor(255, 230, 60, 50), 0 };
            items.push_back(fire);
            if (!e.incident.sarBoats.empty()) {
                Item sar = { french ? L"Moyens SAR" : L"SAR units", irr::video::SColor(255, 40, 200, 190), 0 };
                items.push_back(sar);
            }
            if (!e.incident.survivors.empty()) {
                Item s2 = { french ? L"Naufrag\u00E9s" : L"Survivors", irr::video::SColor(255, 226, 60, 60), 1 };
                items.push_back(s2);
            }
        }
        const irr::f32 lh = Ui::textHeight(fonts.tiny) + 6 * s;
        irr::f32 w = 0;
        for (size_t i = 0; i < items.size(); i++) { w = std::max(w, Ui::textWidth(fonts.tiny, items[i].text)); }
        const irr::core::rect<irr::f32> r(mapArea.UpperLeftCorner.X + 10 * s, mapArea.LowerRightCorner.Y - 14 * s - lh * items.size(),
            mapArea.UpperLeftCorner.X + 10 * s + w + 40 * s, mapArea.LowerRightCorner.Y - 8 * s);
        irr::gui::PanelBatch b;
        b.begin(driver);
        Ui::roundRect(b, r, 8, irr::video::SColor(light ? 215 : 200, 12, 24, 42), irr::video::SColor(light ? 215 : 200, 12, 24, 42));
        for (size_t i = 0; i < items.size(); i++) {
            const irr::core::vector2df c(r.UpperLeftCorner.X + 16 * s, r.UpperLeftCorner.Y + 4 * s + lh * (i + 0.5f));
            if (items[i].kind == 1) { b.disc(c, 4.5f * s, items[i].col, items[i].col); }
            else {
                b.tri(c + irr::core::vector2df(0, -7 * s), c + irr::core::vector2df(4.5f * s, 6 * s), c + irr::core::vector2df(-4.5f * s, 6 * s), items[i].col);
            }
        }
        b.flush();
        for (size_t i = 0; i < items.size(); i++) {
            Ui::drawText(fonts.tiny, items[i].text, irr::core::rect<irr::f32>(r.UpperLeftCorner.X + 30 * s, r.UpperLeftCorner.Y + 4 * s + lh * i,
                r.LowerRightCorner.X, r.UpperLeftCorner.Y + 4 * s + lh * (i + 1)), Ui::text);
        }
    }

    //Facts: a card each for the ship, the conditions, the traffic and the incident.
    struct Fact { std::wstring label, value; };
    struct FactCard { std::wstring title; std::vector<Fact> rows; irr::video::ITexture* picture; std::wstring headline; };

    void drawFacts(irr::video::IVideoDriver* driver, const Exercise& e)
    {
        std::vector<FactCard> cards;
        const std::wstring kn = french ? L" nds" : L" kn";

        FactCard ship;
        ship.title = french ? L"NAVIRE" : L"OWN SHIP";
        ship.headline = shipLabel(e.data.ownShipData.ownShipName);
        ship.picture = shipPicture(e.data.ownShipData.ownShipName);
        Fact f;
        f.label = french ? L"Cap / vitesse" : L"Heading / speed";
        f.value = heading3(e.data.ownShipData.initialBearing) + L"  \u00B7  " + number(e.data.ownShipData.initialSpeed, 1, french) + kn;
        ship.rows.push_back(f);
        f.label = L"Position";
        f.value = position(e.data.ownShipData.initialLat, true) + L"  " + position(e.data.ownShipData.initialLong, false);
        ship.rows.push_back(f);
        if (e.ownShip.known) {
            if (e.ownShip.tonnes > 0) { f.label = french ? L"D\u00E9placement" : L"Displacement"; f.value = number(e.ownShip.tonnes, 0, french) + L" t"; ship.rows.push_back(f); }
            if (e.ownShip.maxSpeed > 0) { f.label = french ? L"Vitesse max." : L"Max. speed"; f.value = number(e.ownShip.maxSpeed, 0, french) + kn; ship.rows.push_back(f); }
            f.label = french ? L"Propulsion" : L"Propulsion";
            f.value = e.ownShip.azimuth ? (french ? L"azimutale" : L"azimuth") : (e.ownShip.twin ? (french ? L"2 moteurs" : L"twin screw") : (french ? L"1 moteur" : L"single screw"));
            if (e.ownShip.bowThruster) { f.value += french ? L", \u00E9trave" : L", bow thruster"; }
            ship.rows.push_back(f);
            if (e.ownShip.fireFighting) { f.label = french ? L"\u00C9quipement" : L"Equipment"; f.value = french ? L"lances incendie" : L"fire monitors"; ship.rows.push_back(f); }
        }
        cards.push_back(ship);

        FactCard cond;
        cond.title = french ? L"CONDITIONS" : L"CONDITIONS";
        cond.picture = 0;
        f.label = french ? L"D\u00E9part" : L"Start";
        f.value = clockTime(e.data.startTime) + L" (" + dayPeriod(e.data) + L")";
        cond.rows.push_back(f);
        f.label = french ? L"Soleil" : L"Sun";
        f.value = (french ? L"lever " : L"rise ") + clockTime(e.data.sunRise > 0 ? e.data.sunRise : 6) + (french ? L", coucher " : L", set ") + clockTime(e.data.sunSet > 0 ? e.data.sunSet : 18);
        cond.rows.push_back(f);
        f.label = french ? L"Vent" : L"Wind";
        f.value = (e.data.windSpeed < 0.5f) ? (french ? L"calme" : L"calm") : compassPoint(e.data.windDirection, french) + L"  " + number(e.data.windSpeed, 0, french) + kn;
        cond.rows.push_back(f);
        f.label = french ? L"Mer" : L"Sea";
        f.value = (e.data.weather < 0.5f) ? (french ? L"calme" : L"calm") : (french ? L"force " : L"force ") + number(e.data.weather, 0, french);
        cond.rows.push_back(f);
        f.label = french ? L"Visibilit\u00E9" : L"Visibility";
        f.value = (e.data.visibilityRange > 0 ? number(e.data.visibilityRange, e.data.visibilityRange < 2 ? 1 : 0, french) : std::wstring(L"?")) + L" NM";
        cond.rows.push_back(f);
        f.label = french ? L"Pluie" : L"Rain";
        const irr::f32 rain = e.data.rainIntensity;
        f.value = rain < 0.5f ? (french ? L"aucune" : L"none") : rain < 3.5f ? (french ? L"faible" : L"light") : rain < 7 ? (french ? L"mod\u00E9r\u00E9e" : L"moderate") : (french ? L"forte" : L"heavy");
        cond.rows.push_back(f);
        cards.push_back(cond);

        FactCard traffic;
        traffic.title = french ? L"TRAFIC" : L"TRAFFIC";
        traffic.picture = 0;
        const size_t n = e.data.otherShipsData.size();
        traffic.headline = (n == 0) ? (french ? L"Aucun autre navire" : L"No other ship")
            : std::to_wstring(n) + (french ? (n > 1 ? L" autres navires" : L" autre navire") : (n > 1 ? L" other ships" : L" other ship"));
        for (size_t i = 0; i < n; i++) {
            const OtherShipData& o = e.data.otherShipsData[i];
            f.label = shipLabel(o.shipName);
            irr::f32 speed = 0;
            for (size_t l = 0; l < o.legs.size(); l++) { if (o.legs[l].distance > 0) { speed = std::max(speed, o.legs[l].speed); } }
            f.value = (speed > 0) ? number(speed, 0, french) + kn : (o.drifting ? (french ? L"d\u00E9rive" : L"drifting") : (french ? L"stopp\u00E9" : L"stopped"));
            traffic.rows.push_back(f);
        }
        cards.push_back(traffic);

        if (e.hasIncident) {
            FactCard inc;
            inc.title = french ? L"INCIDENT" : L"INCIDENT";
            inc.picture = 0;
            const int c = e.incident.casualtyShip;
            inc.headline = (c >= 1 && c <= (int)n) ? (french ? L"Incendie \u00E0 bord : " : L"Fire on board: ") + shipLabel(e.data.otherShipsData[c - 1].shipName)
                : (french ? L"Incendie sur le navire le plus proche" : L"Fire on the nearest ship");
            size_t rafts = 0;
            for (size_t i = 0; i < e.incident.survivors.size(); i++) { if (e.incident.survivors[i].kind != Survivor_MOB) { rafts++; } }
            f.label = french ? L"Naufrag\u00E9s" : L"Survivors";
            f.value = std::to_wstring(e.incident.survivors.size() - rafts) + (french ? L" \u00E0 l'eau, " : L" in the water, ")
                + countOf(rafts, french ? L"radeau" : L"raft", french ? L"radeaux" : L"rafts");
            inc.rows.push_back(f);
            f.label = french ? L"Moyens SAR" : L"SAR units";
            f.value = countOf(e.incident.sarBoats.size(), french ? L"vedette" : L"boat", french ? L"vedettes" : L"boats") + L", "
                + countOf(e.incident.helos.size(), french ? L"h\u00E9licopt\u00E8re" : L"helicopter", french ? L"h\u00E9licopt\u00E8res" : L"helicopters");
            inc.rows.push_back(f);
            if (!e.incident.coordinationCentre.empty()) { f.label = french ? L"Coordination" : L"Coordination"; f.value = decodeText(e.incident.coordinationCentre); inc.rows.push_back(f); }
            cards.push_back(inc);
        }

        //Lay the cards out: stacked in the column, or side by side in the row
        const irr::f32 lh = Ui::textHeight(fonts.small) + 7 * s;
        std::vector<irr::core::rect<irr::f32> > areas;
        if (wide) {
            std::vector<irr::f32> heights(cards.size());
            irr::f32 total = 0;
            for (size_t i = 0; i < cards.size(); i++) {
                heights[i] = 44 * s + (cards[i].headline.empty() ? 0 : lh + 4 * s) + lh * cards[i].rows.size() + 12 * s;
                total += heights[i] + gap;
            }
            //The ship's picture only if every card still fits with it
            for (size_t i = 0; i < cards.size(); i++) {
                if (!cards[i].picture) { continue; }
                const irr::f32 ph = pictureHeight(cards[i].picture, infoArea.getWidth() - 32 * s) + 10 * s;
                if (total + ph <= infoArea.getHeight() + gap) { heights[i] += ph; total += ph; }
                else { cards[i].picture = 0; }
            }
            irr::f32 y = infoArea.UpperLeftCorner.Y;
            for (size_t i = 0; i < cards.size(); i++) {
                irr::f32 h = heights[i];
                const irr::f32 room = infoArea.LowerRightCorner.Y - y;
                if (room < 90 * s) { break; }
                h = std::min(h, room);
                areas.push_back(irr::core::rect<irr::f32>(infoArea.UpperLeftCorner.X, y, infoArea.LowerRightCorner.X, y + h));
                y += h + gap;
            }
        }
        else {
            //Three cards in the row: the incident rather than the traffic (which the chart shows)
            if (cards.size() > 3) { cards.erase(cards.begin() + 2); }
            const size_t count = std::min(cards.size(), (size_t)3);
            const irr::f32 w = (infoArea.getWidth() - gap * (count - 1)) / count;
            for (size_t i = 0; i < count; i++) {
                cards[i].picture = 0; //no room for the picture in a row
                areas.push_back(irr::core::rect<irr::f32>(infoArea.UpperLeftCorner.X + i * (w + gap), infoArea.UpperLeftCorner.Y,
                    infoArea.UpperLeftCorner.X + i * (w + gap) + w, infoArea.LowerRightCorner.Y));
            }
        }
        irr::gui::PanelBatch b;
        b.begin(driver);
        for (size_t i = 0; i < areas.size(); i++) { Ui::card(b, areas[i], 14); }
        b.flush();
        for (size_t i = 0; i < areas.size(); i++) {
            const FactCard& c = cards[i];
            const irr::core::rect<irr::f32>& r = areas[i];
            const irr::core::rect<irr::s32> clip = Ui::toI(r);
            const irr::f32 x0 = r.UpperLeftCorner.X + 16 * s, x1 = r.LowerRightCorner.X - 16 * s;
            irr::f32 y = r.UpperLeftCorner.Y + 12 * s;
            Ui::drawText(fonts.tiny, c.title, irr::core::rect<irr::f32>(x0, y, x1, y + 20 * s), Ui::accentHi, Ui::Left, &clip);
            y += 28 * s;
            if (c.picture) {
                const irr::f32 ph = pictureHeight(c.picture, x1 - x0);
                const irr::core::dimension2du size = c.picture->getOriginalSize();
                driver->draw2DImage(c.picture, irr::core::recti((irr::s32)x0, (irr::s32)y, (irr::s32)x1, (irr::s32)(y + ph)),
                    irr::core::recti(0, 0, (irr::s32)size.Width, (irr::s32)size.Height), &clip, 0, true);
                y += ph + 10 * s;
            }
            if (!c.headline.empty()) {
                Ui::drawText(fonts.text, c.headline, irr::core::rect<irr::f32>(x0, y, x1, y + lh), Ui::text, Ui::Left, &clip);
                y += lh + 4 * s;
            }
            //The lines that fit; if some do not, the last line says how many more there are
            const size_t fit = (size_t)std::max(0.0f, (r.LowerRightCorner.Y - 6 * s - y) / lh);
            const size_t drawn = (fit >= c.rows.size()) ? c.rows.size() : (fit > 0 ? fit - 1 : 0);
            if (drawn < c.rows.size() && fit > 0) {
                const std::wstring more = L"+" + std::to_wstring(c.rows.size() - drawn) + (french ? L" autres" : L" more");
                Ui::drawText(fonts.small, more, irr::core::rect<irr::f32>(x0, y + drawn * lh, x1, y + (drawn + 1) * lh), Ui::textFaint, Ui::Left, &clip);
            }
            for (size_t k = 0; k < drawn; k++) {
                //The value gets the room it needs, leaving the label at least a third of the line
                const irr::f32 fullW = Ui::textWidth(fonts.small, c.rows[k].value);
                const irr::f32 labelMin = std::min(Ui::textWidth(fonts.small, c.rows[k].label), (x1 - x0) * 0.34f);
                const irr::f32 valueW = std::min(fullW, x1 - x0 - labelMin - 8 * s);
                const irr::core::rect<irr::s32> labelClip((irr::s32)x0, clip.UpperLeftCorner.Y, (irr::s32)(x1 - valueW - 8 * s), clip.LowerRightCorner.Y);
                const irr::core::rect<irr::s32> valueClip((irr::s32)(x1 - valueW), clip.UpperLeftCorner.Y, (irr::s32)x1, clip.LowerRightCorner.Y);
                Ui::drawText(fonts.small, c.rows[k].label, irr::core::rect<irr::f32>(x0, y, x1 - valueW - 8 * s, y + lh), Ui::textDim, Ui::Left, &labelClip);
                Ui::drawText(fonts.small, c.rows[k].value, irr::core::rect<irr::f32>(x1 - valueW, y, x1, y + lh), Ui::text, fullW > valueW ? Ui::Left : Ui::Right, &valueClip);
                y += lh;
            }
        }
    }

    irr::f32 pictureHeight(irr::video::ITexture* t, irr::f32 width) const
    {
        const irr::core::dimension2du d = t->getOriginalSize();
        if (d.Width == 0) { return 0; }
        return std::min(width * d.Height / d.Width, 150 * s);
    }

    //boat_pictures/<model>.png, as the scenario editor shows it; 0 if there is none.
    irr::video::ITexture* shipPicture(const std::string& model)
    {
        std::map<std::string, irr::video::ITexture*>::iterator it = pictures.find(model);
        if (it != pictures.end()) { return it->second; }
        irr::video::ITexture* t = 0;
        const std::string path = "boat_pictures/" + model + ".png";
        if (!model.empty() && Utilities::pathExists(path)) { t = Environment->getVideoDriver()->getTexture(path.c_str()); }
        pictures[model] = t;
        return t;
    }

    void drawDescription(const Exercise& e)
    {
        const irr::core::rect<irr::s32> clip = Ui::toI(descCard);
        const irr::f32 x0 = descCard.UpperLeftCorner.X + 18 * s, x1 = descCard.LowerRightCorner.X - 18 * s;
        irr::f32 y = descCard.UpperLeftCorner.Y + 12 * s;
        Ui::drawText(fonts.tiny, french ? L"DESCRIPTION" : L"DESCRIPTION", irr::core::rect<irr::f32>(x0, y, x1, y + 20 * s), Ui::accentHi, Ui::Left, &clip);
        y += 28 * s;
        if (e.description.empty()) {
            Ui::drawText(fonts.text, french ? L"Pas de description pour cet exercice (\u00E0 ajouter dans l'\u00E9diteur de sc\u00E9nario)."
                : L"No description for this exercise (it can be added in the scenario editor).", irr::core::rect<irr::f32>(x0, y, x1, y + 24 * s), Ui::textFaint, Ui::Left, &clip);
            return;
        }
        const std::vector<std::wstring> lines = Ui::wrap(fonts.text, e.description, x1 - x0);
        const irr::f32 lh = Ui::textHeight(fonts.text) + 3 * s;
        for (size_t i = 0; i < lines.size(); i++) {
            if (y + lh > descCard.LowerRightCorner.Y - 8 * s) {
                Ui::drawText(fonts.text, L"\u2026", irr::core::rect<irr::f32>(x0, y - lh, x1, y), Ui::textDim, Ui::Right, &clip);
                break;
            }
            Ui::drawText(fonts.text, lines[i], irr::core::rect<irr::f32>(x0, y, x1, y + lh), Ui::text, Ui::Left, &clip);
            y += lh;
        }
    }

    //-----------------------------------------------------------------------------------------------
    //Drawing: secondary display and multiplayer
    //-----------------------------------------------------------------------------------------------

    void drawStationMode(irr::video::IVideoDriver* driver)
    {
        irr::gui::PanelBatch b;
        b.begin(driver);
        Ui::card(b, stationCard, 16);
        const irr::core::vector2df icon(stationCard.UpperLeftCorner.X + 64 * s, stationCard.UpperLeftCorner.Y + 66 * s);
        b.disc(icon, 30 * s, irr::video::SColor(60, 64, 156, 240), irr::video::SColor(60, 64, 156, 240));
        Ui::networkIcon(b, icon, 18 * s, Ui::accentHi);
        field(b, portBox);
        b.flush();
        const irr::f32 x0 = stationCard.UpperLeftCorner.X + 112 * s, x1 = stationCard.LowerRightCorner.X - 36 * s;
        const bool secondary = (mode == ModeSecondary);
        Ui::drawText(fonts.heading, secondary ? (french ? L"Ce poste affiche la vue d'un autre poste" : L"This station shows another station's view")
            : (french ? L"Ce poste rejoint un exercice multijoueur" : L"This station joins a multiplayer exercise"),
            irr::core::rect<irr::f32>(x0, stationCard.UpperLeftCorner.Y + 40 * s, x1, stationCard.UpperLeftCorner.Y + 80 * s), Ui::text);
        Ui::drawWrapped(fonts.text, stationText(), irr::core::rect<irr::f32>(x0, stationCard.UpperLeftCorner.Y + 92 * s, x1, portField.UpperLeftCorner.Y - 30 * s), Ui::textDim, 4 * s);
        Ui::drawText(fonts.small, french ? L"Port UDP d'\u00E9coute" : L"UDP port to listen on",
            irr::core::rect<irr::f32>(portField.UpperLeftCorner.X, portField.UpperLeftCorner.Y - 26 * s, x1, portField.UpperLeftCorner.Y - 4 * s), Ui::textDim);
        Ui::drawText(fonts.small, french ? L"18304 sauf si un autre programme l'utilise d\u00E9j\u00E0 sur ce PC." : L"18304 unless another program already uses it on this PC.",
            irr::core::rect<irr::f32>(portField.LowerRightCorner.X + 16 * s, portField.UpperLeftCorner.Y, x1, portField.LowerRightCorner.Y), Ui::textFaint);
    }

    //"1 vedette", "3 vedettes"
    static std::wstring countOf(size_t n, const wchar_t* one, const wchar_t* many)
    {
        return std::to_wstring(n) + L" " + (n == 1 ? one : many);
    }

    std::wstring stationText() const
    {
        const bool secondary = (mode == ModeSecondary);
        return secondary
            ? (french ? L"Le poste principal fait tourner l'exercice et envoie la position du navire \u00E0 ce poste, qui affiche une autre vue (vue lat\u00E9rale, grand radar\u2026). Sur le poste principal, indiquez le nom ou l'adresse de ce PC dans \u00AB Envoyer les donn\u00E9es \u00E0 \u00BB."
                : L"The main station runs the exercise and sends the ship's position to this station, which shows another view (side view, large radar\u2026). On the main station, enter this PC's name or address in \u00AB Send the data to \u00BB.")
            : (french ? L"L'exercice et le navire de chaque poste sont choisis dans le Hub multijoueurs, qui lance l'exercice. Ce poste attend le hub sur le port ci-dessous. Les affichages secondaires de ce poste peuvent \u00EAtre indiqu\u00E9s en bas de l'\u00E9cran."
                : L"The exercise and each station's ship are chosen in the Multiplayer hub, which starts the exercise. This station waits for the hub on the port below. This station's secondary displays can be entered at the bottom of the screen.");
    }

    //-----------------------------------------------------------------------------------------------
    //Small drawing pieces
    //-----------------------------------------------------------------------------------------------

    //Rounded field behind an edit box (the box itself draws only its text).
    void field(irr::gui::PanelBatch& b, irr::gui::IGUIEditBox* box)
    {
        irr::core::rect<irr::f32> r = Ui::toF(box->getAbsolutePosition());
        if (box == search) { r = searchField; }
        else { r.UpperLeftCorner.X -= 10 * s; r.LowerRightCorner.X += 6 * s; }
        const bool focus = Environment->hasFocus(box);
        Ui::roundRect(b, r, 8, Ui::field, Ui::field);
        Ui::roundRectOutline(b, r, 8, focus ? 1.5f : 1.0f, focus ? irr::video::SColor(255, 90, 170, 255) : irr::video::SColor(120, 110, 150, 210));
    }

    void drawSearchIcon(irr::gui::PanelBatch& b, irr::core::vector2df c, irr::f32 r)
    {
        b.sector(c, r - 1.8f, r, 0, 360, Ui::textDim, Ui::textDim);
        b.line(c + irr::core::vector2df(r * 0.7f, r * 0.7f), c + irr::core::vector2df(r * 1.5f, r * 1.5f), 2.0f, Ui::textDim);
    }

    void drawHelm(irr::gui::PanelBatch& b, irr::core::vector2df c, irr::f32 sz, irr::video::SColor col)
    {
        const irr::f32 lw = std::max(1.6f, sz * 0.13f);
        b.sector(c, sz * 0.50f, sz * 0.50f + lw, 0, 360, col, col);
        b.disc(c, sz * 0.20f, col, col);
        for (int i = 0; i < 8; i++) {
            const irr::f32 a = 22.5f + 45.0f * i;
            b.line(irr::gui::panelPolar(c, sz * 0.18f, a), irr::gui::panelPolar(c, sz * 0.86f, a), lw, col);
            b.disc(irr::gui::panelPolar(c, sz * 0.92f, a), lw * 0.85f, col, col);
        }
    }

    struct Badge {
        std::wstring text;
        irr::core::rect<irr::f32> area;
        Badge(const std::wstring& t, const irr::core::rect<irr::f32>& a) : text(t), area(a) {}
    };

    irr::IrrlichtDevice* device;
    std::vector<Exercise>& exercises;
    bool french;
    std::string fontName;
    Fonts fonts;
    Mode mode;

    irr::gui::IGUIEditBox* search;
    irr::gui::IGUIEditBox* hostnameBox;
    irr::gui::IGUIEditBox* portBox;
    Ui::Button* modeButtons[3];
    Ui::Button* startButton;
    Ui::Button* quitButton;
    Ui::Button* zoomIn;
    Ui::Button* zoomOut;
    Ui::Button* zoomFit;

    std::vector<size_t> shown;   //exercises matching the search
    int selected, hovered;       //rows in 'shown'
    irr::f32 scroll;
    bool started;
    int mapFor;                  //exercise the chart is fitted on
    bool chartPending;
    std::map<std::string, ChartView*> charts;      //per area (world), 0 when it could not be loaded
    std::map<std::string, std::wstring> chartErrors;
    std::map<std::string, irr::video::ITexture*> pictures;
    std::vector<Badge> badges;

    bool dragging;
    irr::core::position2di dragFrom;
    irr::core::vector2df mouse;
    irr::u32 lastClickMs;
    int lastClickRow;

    irr::core::dimension2du laidOutFor;
    irr::f32 s, pad, gap, headerH, footerH, rowH;
    bool wide;
    irr::core::rect<irr::f32> body, listCard, searchField, listArea, titleArea, mapCard, mapArea, infoArea, descCard, stationCard, portField, startArea;
};

std::string lastExerciseFile()
{
    return Utilities::getUserDir() + "lastScenario.ini";
}

} // namespace

ScenarioChoice::ScenarioChoice(irr::IrrlichtDevice* device, Lang* language, const std::string& fontName, bool french)
    : device(device), gui(device->getGUIEnvironment()), language(language), fontName(fontName), french(french)
{
}

void ScenarioChoice::chooseScenario(std::string& scenarioName, std::string& hostname, irr::u32& udpPort, OperatingMode::Mode& mode, std::string scenarioPath)
{
    irr::video::IVideoDriver* driver = device->getVideoDriver();

    //Skin: the colours the simulator's own controls have used since this screen first set them, kept for
    //after it; the screen itself uses the navy theme.
    irr::gui::IGUISkin* skin = gui->getSkin();
    skin->setColor(irr::gui::EGDC_WINDOW_SYMBOL, irr::video::SColor(255, 255, 255, 255));
    skin->setColor(irr::gui::EGDC_WINDOW, irr::video::SColor(255, 0, 3, 52));
    skin->setColor(irr::gui::EGDC_3D_FACE, irr::video::SColor(255, 0, 3, 52));
    skin->setColor(irr::gui::EGDC_3D_SHADOW, irr::video::SColor(255, 131, 138, 255));
    skin->setColor(irr::gui::EGDC_BUTTON_TEXT, irr::video::SColor(255, 255, 255, 255));
    skin->setColor(irr::gui::EGDC_HIGH_LIGHT_TEXT, irr::video::SColor(255, 236, 255, 0));
    skin->setColor(irr::gui::EGDC_GRAY_TEXT, irr::video::SColor(255, 200, 200, 200));
    skin->setColor(irr::gui::EGDC_HIGH_LIGHT, irr::video::SColor(255, 0, 0, 0));
    skin->setColor(irr::gui::EGDC_ACTIVE_BORDER, irr::video::SColor(255, 100, 100, 100));
    skin->setColor(irr::gui::EGDC_INACTIVE_BORDER, irr::video::SColor(255, 70, 70, 70));
    skin->setColor(irr::gui::EGDC_EDITABLE, irr::video::SColor(255, 30, 30, 50));
    skin->setColor(irr::gui::EGDC_FOCUSED_EDITABLE, irr::video::SColor(255, 30, 30, 50));
    irr::video::SColor kept[irr::gui::EGDC_COUNT];
    for (int i = 0; i < irr::gui::EGDC_COUNT; i++) { kept[i] = skin->getColor((irr::gui::EGUI_DEFAULT_COLOR)i); }
    Ui::applySkin(skin);

    std::vector<Exercise> exercises = loadExercises(device, scenarioPath);
    const std::string lastExercise = IniFile::iniFileToString(lastExerciseFile(), "Scenario");

    std::wstring wHostname;
    for (size_t i = 0; i < hostname.size(); i++) { wHostname += (wchar_t)(unsigned char)hostname[i]; }
    ScenarioScreen* screen = new ScenarioScreen(device, exercises, french, fontName, wHostname, udpPort, lastExercise);

    StartupEventReceiver startupReceiver(screen, device);
    irr::IEventReceiver* oldReceiver = device->getEventReceiver();
    device->setEventReceiver(&startupReceiver);

    while (device->run() && !screen->isStarted()) {
        screen->beforeFrame();
        driver->beginScene(irr::video::ECBF_COLOR | irr::video::ECBF_DEPTH, Ui::backgroundDeep);
        gui->drawAll();
        driver->endScene();
        device->sleep(4); //an idle screen: no need for every frame the card can draw
    }

    const bool started = screen->isStarted();
    const int chosen = screen->chosenExercise();
    const ScenarioScreen::Mode chosenMode = screen->getMode();
    const std::wstring wHost = screen->hostname();
    const std::wstring wPort = screen->port();
    device->setEventReceiver(oldReceiver); //the startup receiver is about to go
    screen->remove();
    screen->drop();
    for (int i = 0; i < irr::gui::EGDC_COUNT; i++) { skin->setColor((irr::gui::EGUI_DEFAULT_COLOR)i, kept[i]); }

    if (!started) {
        ExitMessage::exitWithMessage("No scenario selected.");
    }

    //Hostname(s), one byte per character as before
    hostname.clear();
    for (size_t i = 0; i < wHost.size(); i++) { hostname += (char)wHost[i]; }

    if (chosenMode == ScenarioScreen::ModeSecondary) { mode = OperatingMode::Secondary; }
    else if (chosenMode == ScenarioScreen::ModeMultiplayer) { mode = OperatingMode::Multiplayer; }
    else { mode = OperatingMode::Normal; }

    if (mode != OperatingMode::Normal) {
        const irr::u32 port = (irr::u32)std::wcstoul(wPort.c_str(), 0, 10);
        if (port > 0 && port < 65536) { udpPort = port; }
    }
    else {
        if (chosen < 0) { ExitMessage::exitWithMessage("No scenario selected."); }
        scenarioName = exercises[chosen].folder;
        std::ofstream last(lastExerciseFile().c_str());
        if (last) { last << "Scenario=\"" << scenarioName << "\"" << std::endl; }
    }
}
