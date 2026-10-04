/* SCENARIO INCENDIE - fire scenario editor: the editing screen. See FireEditor.hpp.
   Source kept ASCII: accented text is written with \u escapes so every compiler reads it the same. */
#include "FireEditor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <locale>
#include <sstream>

#include "../Utilities.hpp"
#include "../chartView/ChartDraw.hpp"

namespace {

enum {
    ID_NAME = 1000, ID_SAVE, ID_MENU, ID_TABS,
    ID_DAY, ID_MONTH, ID_YEAR, ID_START, ID_SUNRISE, ID_SUNSET, ID_WEATHER, ID_RAIN, ID_VIS, ID_WINDDIR, ID_WINDSPD, ID_DESC,
    ID_OWN_TYPE, ID_OWN_HDG, ID_OWN_SPD, ID_OWN_PLACE,
    ID_CAS_TYPE, ID_CAS_HDG, ID_CAS_PLACE, ID_CAS_MOVEGROUP,
    ID_T_ABANDON, ID_T_SPREAD, ID_T_DURATION, ID_T_SINKLEAD, ID_T_PERMLIST, ID_T_INTERVAL,
    ID_SURV_LIST, ID_SURV_ADD_MOB, ID_SURV_ADD_LIFERAFT, ID_SURV_ADD_RADEAU, ID_SURV_DELETE, ID_SURV_UP, ID_SURV_DOWN,
    ID_SURV_BOAT, ID_SURV_DEFAULT,
    ID_BOAT_LIST, ID_BOAT_ADD, ID_BOAT_DELETE, ID_BOAT_TYPE, ID_BOAT_HDG, ID_BOAT_SPEED, ID_BOAT_DELAY, ID_BOAT_MOOR,
    ID_BOAT_PLACE, ID_BOAT_OUT, ID_BOAT_OUT_CLEAR, ID_BOAT_RET, ID_BOAT_RET_CLEAR,
    ID_HELO_DELAY, ID_HELO_SPEED, ID_HELO_LIST, ID_HELO_ADD, ID_HELO_DELETE, ID_HELO_MODEL, ID_HELO_BASE, ID_HELO_BASE_CLEAR,
    ID_HELO_BASE_H, ID_HELO_PAD, ID_HELO_PAD_CLEAR, ID_HELO_PAD_H,
    ID_CONFIRM_OVERWRITE, ID_CONFIRM_MENU, ID_MRSC, ID_STYLE, ID_CONFIRM_TIMELINE
};

enum { TAB_SCENARIO = 0, TAB_CASUALTY = 1, TAB_SURVIVORS = 2, TAB_BOATS = 3, TAB_HELOS = 4 };
enum { Shape_Circle = 0, Shape_Square = 1, Shape_Diamond = 2 };

const irr::video::SColor kWhite(255, 255, 255, 255);
const irr::video::SColor kBlack(255, 0, 0, 0);
const irr::video::SColor kOwnFill(255, 70, 140, 255);
const irr::video::SColor kCasualtyFill(255, 220, 30, 30);
const irr::video::SColor kTrafficFill(255, 150, 150, 150);
const irr::video::SColor kMobFill(255, 255, 90, 40);
const irr::video::SColor kHeloColour(255, 196, 130, 255);
const irr::video::SColor kSelect(255, 0, 255, 255);
const irr::video::SColor kHover(180, 255, 255, 255);

// Latin-1 widening / narrowing, as the standard editor does for names and descriptions.
std::wstring wide(const std::string& s)
{
    std::wstring w;
    for (size_t i = 0; i < s.size(); i++) { w += (wchar_t)(unsigned char)s[i]; }
    return w;
}

std::string narrow(const std::wstring& w)
{
    std::string s;
    for (size_t i = 0; i < w.size(); i++) { s += (w[i] < 256) ? (char)w[i] : '?'; }
    return s;
}

std::wstring trimW(const std::wstring& s)
{
    size_t b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) { return L""; }
    size_t e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool parseNumber(const std::wstring& text, double& out)
{
    std::string s = narrow(trimW(text));
    if (s.empty()) { return false; }
    std::replace(s.begin(), s.end(), ',', '.');
    std::istringstream in(s);
    in.imbue(std::locale::classic());
    double v;
    if (!(in >> v)) { return false; }
    std::string rest;
    if (in >> rest) { return false; }
    out = v;
    return true;
}

// "mm:ss" or plain seconds.
bool parseTime(const std::wstring& text, float& secs)
{
    std::wstring t = trimW(text);
    size_t colon = t.find(L':');
    if (colon == std::wstring::npos) {
        double v;
        if (!parseNumber(t, v) || v < 0) { return false; }
        secs = (float)v;
        return true;
    }
    double m, s;
    if (!parseNumber(t.substr(0, colon), m) || !parseNumber(t.substr(colon + 1), s)) { return false; }
    if (m < 0 || s < 0 || s >= 60) { return false; }
    secs = (float)(m * 60.0 + s);
    return true;
}

std::wstring fmtTime(float secs)
{
    int total = (int)std::floor(secs + 0.5f);
    if (total < 0) { total = 0; }
    wchar_t buf[32];
    swprintf(buf, 32, L"%02d:%02d", total / 60, total % 60);
    return buf;
}

std::wstring fmtNum(double v, int decimals)
{
    wchar_t buf[64];
    if (decimals <= 0) { swprintf(buf, 64, L"%.0f", v); }
    else if (decimals == 1) { swprintf(buf, 64, L"%.1f", v); }
    else { swprintf(buf, 64, L"%.2f", v); }
    return buf;
}

std::wstring fmtLatLong(const IncidentPoint& p)
{
    double lat = std::fabs(p.lat), lon = std::fabs(p.lon);
    wchar_t buf[96];
    swprintf(buf, 96, L"%02d\u00B0%06.3f'%lc  %03d\u00B0%06.3f'%lc",
        (int)lat, (lat - (int)lat) * 60.0, p.lat >= 0 ? L'N' : L'S',
        (int)lon, (lon - (int)lon) * 60.0, p.lon >= 0 ? L'E' : L'W');
    return buf;
}

} // namespace

FireEditor::FireEditor(irr::IrrlichtDevice* dev, FireScenario* scenario, ChartView* chart, const std::string& path,
    const std::vector<std::string>& ownShipTypes, const std::vector<std::string>& otherShipTypes,
    const std::vector<std::string>& rescueShipTypes, bool isNewScenario)
    : device(dev), scn(scenario), map(chart), scenariosPath(path), ownTypes(ownShipTypes), otherTypes(otherShipTypes),
    rescueTypes(rescueShipTypes),
    quit(false), backToMenu(false), dirty(isNewScenario), needRefresh(false), tool(Tool_Select), selBoat(-1), selHelo(-1), selSurvivor(-1),
    moveGroupWithCasualty(true), helpField(FireHelp::Field_None), timelineConfirmed(false),
    leftDown(false), rightDown(false), panning(false), dragging(false), rightMoved(false)
{
    driver = device->getVideoDriver();
    guienv = device->getGUIEnvironment();
    font = guienv->getSkin()->getFont();
    loadedName = isNewScenario ? "" : scn->name;   // a new exercise must not silently replace an existing one

    screenW = (irr::s32)driver->getScreenSize().Width;
    screenH = (irr::s32)driver->getScreenSize().Height;
    irr::s32 panelW = std::max(400, (irr::s32)(screenW * 0.34f));
    if (panelW > screenW - 300) { panelW = screenW - 300; }
    panelX = screenW - panelW;
    rowH = std::max(24, (irr::s32)font->getDimension(L"Ag").Height + 10);

    map->setViewport(irr::core::recti(0, 0, panelX, screenH));

    if (!scn->sarBoats.empty()) { selBoat = 0; }
    if (!scn->incident.helos.empty()) { selHelo = 0; }

    // Frame the exercise: centred on the casualty, wide enough to show everything placed.
    IncidentPoint focus = scn->hasCasualty ? scn->casualty.pos : scn->ownShip.pos;
    map->centreOn(focus);
    double far = 300.0;
    std::vector<IncidentPoint> pts;
    pts.push_back(scn->ownShip.pos);
    for (size_t i = 0; i < scn->sarBoats.size(); i++) { pts.push_back(scn->sarBoats[i].pos); }
    for (size_t i = 0; i < scn->incident.survivors.size(); i++) { pts.push_back(scn->incident.survivors[i].pos); }
    for (size_t i = 0; i < pts.size(); i++) { far = std::max(far, map->metresBetween(focus, pts[i])); }
    map->setMetresPerPixel(far * 2.4 / std::min(panelX, screenH));

    buildGui();
    refreshAll();
}

FireEditor::~FireEditor()
{
}

// ------------------------------------------------------------------------------------------
// Panel construction

irr::gui::IGUIStaticText* FireEditor::label(irr::gui::IGUIElement* parent, irr::s32 x, irr::s32 y, irr::s32 w, const wchar_t* text)
{
    irr::gui::IGUIStaticText* t = guienv->addStaticText(text, irr::core::recti(x, y + 3, x + w, y + rowH), false, false, parent);
    return t;
}

irr::gui::IGUIEditBox* FireEditor::edit(irr::gui::IGUIElement* parent, irr::s32 x, irr::s32 y, irr::s32 w, irr::s32 id)
{
    return guienv->addEditBox(L"", irr::core::recti(x, y, x + w, y + rowH - 4), true, parent, id);
}

irr::gui::IGUIButton* FireEditor::button(irr::gui::IGUIElement* parent, irr::s32 x, irr::s32 y, irr::s32 w, irr::s32 id, const wchar_t* text)
{
    return guienv->addButton(irr::core::recti(x, y, x + w, y + rowH - 2), parent, id, text);
}

irr::gui::IGUIComboBox* FireEditor::combo(irr::gui::IGUIElement* parent, irr::s32 x, irr::s32 y, irr::s32 w, irr::s32 id)
{
    return guienv->addComboBox(irr::core::recti(x, y, x + w, y + rowH - 4), parent, id);
}

void FireEditor::fillTypeCombo(irr::gui::IGUIComboBox* box, const std::vector<std::string>& types, const std::string& current)
{
    box->clear();
    int sel = -1;
    for (size_t i = 0; i < types.size(); i++) {
        box->addItem(wide(types[i]).c_str());
        if (types[i] == current) { sel = (int)i; }
    }
    if (sel < 0 && !current.empty()) {
        // Model not installed on this computer: keep it, so saving does not change the scenario.
        sel = (int)box->addItem(wide(current).c_str());
    }
    box->setSelected(sel);
}

void FireEditor::buildGui()
{
    irr::s32 x0 = panelX + 10;
    irr::s32 pw = screenW - panelX - 20;
    irr::s32 y = 6;

    label(0, x0, y, pw, L"Nom de l'exercice");
    y += rowH - 4;
    nameBox = guienv->addEditBox(wide(scn->name).c_str(), irr::core::recti(x0, y, x0 + pw, y + rowH - 2), true, 0, ID_NAME);
    y += rowH + 2;
    irr::s32 bw = (pw - 10) / 2;
    button(0, x0, y, bw, ID_SAVE, L"Enregistrer");
    button(0, x0 + bw + 10, y, bw, ID_MENU, L"Retour au menu");
    y += rowH + 2;
    statusText = guienv->addStaticText(L"", irr::core::recti(x0, y, x0 + pw, y + rowH * 2 - 6), false, true, 0);
    styleButton = guienv->addButton(irr::core::recti(panelX - 270, rowH + 12, panelX - 12, rowH * 2 + 10), 0, ID_STYLE,
        (L"Fond : " + map->styleName()).c_str(), L"Changer le fond de carte : carte marine jour / nuit, carte d'origine, image HD");
    y += rowH * 2 - 4;

    tabs = guienv->addTabControl(irr::core::recti(panelX + 4, y, screenW - 4, screenH - 4), 0, true, true, ID_TABS);
    tabScenario = tabs->addTab(L"Sc\u00E9nario");
    tabCasualty = tabs->addTab(L"Sinistre");
    tabSurvivors = tabs->addTab(L"Naufrag\u00E9s");
    tabBoats = tabs->addTab(L"Vedettes SAR");
    tabHelos = tabs->addTab(L"H\u00E9licos");

    const irr::s32 tw = (screenW - 4) - (panelX + 4) - 16;   // usable width inside a tab
    const irr::s32 lx = 8;                                     // label column
    const irr::s32 lw = (irr::s32)(tw * 0.60f);
    const irr::s32 fx = lx + lw + 4;                           // field column
    const irr::s32 fw = tw - lw - 4;
    const irr::s32 tabH = screenH - 4 - y - tabs->getTabHeight() - 8;

    // --- Scenario: date, weather, own ship --------------------------------------------------
    irr::gui::IGUIElement* t = tabScenario;
    y = 8;
    label(t, lx, y, lw, L"Date (jour / mois / ann\u00E9e)");
    irr::s32 dw = (fw - 8) / 3;
    dayBox = edit(t, fx, y, dw, ID_DAY);
    monthBox = edit(t, fx + dw + 4, y, dw, ID_MONTH);
    yearBox = edit(t, fx + 2 * (dw + 4), y, fw - 2 * (dw + 4), ID_YEAR);
    y += rowH;
    label(t, lx, y, lw, L"Heure de d\u00E9but (hh:mm)");
    startBox = edit(t, fx, y, fw, ID_START);
    y += rowH;
    label(t, lx, y, lw, L"Lever / coucher du soleil (h)");
    irr::s32 hw = (fw - 4) / 2;
    sunriseBox = edit(t, fx, y, hw, ID_SUNRISE);
    sunsetBox = edit(t, fx + hw + 4, y, fw - hw - 4, ID_SUNSET);
    y += rowH;
    label(t, lx, y, lw, L"M\u00E9t\u00E9o / \u00E9tat de la mer (0 \u00E0 12)");
    weatherBox = edit(t, fx, y, fw, ID_WEATHER);
    y += rowH;
    label(t, lx, y, lw, L"Pluie (0 \u00E0 10)");
    rainBox = edit(t, fx, y, fw, ID_RAIN);
    y += rowH;
    label(t, lx, y, lw, L"Visibilit\u00E9 (NM)");
    visBox = edit(t, fx, y, fw, ID_VIS);
    y += rowH;
    label(t, lx, y, lw, L"Vent : direction (\u00B0) / force (nds)");
    windDirBox = edit(t, fx, y, hw, ID_WINDDIR);
    windSpdBox = edit(t, fx + hw + 4, y, fw - hw - 4, ID_WINDSPD);
    y += rowH;
    label(t, lx, y, lw, L"Centre SAR local (journal radio)");
    mrscBox = edit(t, fx, y, fw, ID_MRSC);
    y += rowH;
    label(t, lx, y, tw, L"Description");
    y += rowH - 6;
    descBox = guienv->addEditBox(L"", irr::core::recti(lx, y, lx + tw, y + rowH * 3), true, t, ID_DESC);
    descBox->setMultiLine(true);
    descBox->setWordWrap(true);
    descBox->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_UPPERLEFT);
    y += rowH * 3 + 10;
    irr::gui::IGUIStaticText* h = label(t, lx, y, tw, L"NAVIRE PROPRE (stagiaire)");
    h->setOverrideColor(irr::video::SColor(255, 120, 190, 255));
    y += rowH;
    label(t, lx, y, 80, L"Mod\u00E8le");
    ownTypeBox = combo(t, lx + 84, y, tw - 84, ID_OWN_TYPE);
    y += rowH;
    label(t, lx, y, lw, L"Cap initial (\u00B0) / vitesse (nds)");
    ownHdgBox = edit(t, fx, y, hw, ID_OWN_HDG);
    ownSpdBox = edit(t, fx + hw + 4, y, fw - hw - 4, ID_OWN_SPD);
    y += rowH;
    button(t, lx, y, tw, ID_OWN_PLACE, L"Placer le navire propre sur la carte");
    y += rowH + 4;
    trafficText = guienv->addStaticText(L"", irr::core::recti(lx, y, lx + tw, tabH), false, true, t);

    // --- Casualty and timeline ---------------------------------------------------------------
    t = tabCasualty;
    y = 8;
    h = label(t, lx, y, tw, L"NAVIRE EN FEU");
    h->setOverrideColor(irr::video::SColor(255, 255, 110, 90));
    y += rowH;
    label(t, lx, y, 80, L"Mod\u00E8le");
    casTypeBox = combo(t, lx + 84, y, tw - 84, ID_CAS_TYPE);
    y += rowH;
    label(t, lx, y, lw, L"Cap (\u00B0)");
    casHdgBox = edit(t, fx, y, fw, ID_CAS_HDG);
    y += rowH;
    button(t, lx, y, tw, ID_CAS_PLACE, L"Placer le navire en feu sur la carte");
    y += rowH;
    moveGroupBox = guienv->addCheckBox(true, irr::core::recti(lx, y, lx + tw, y + rowH - 2), t, ID_CAS_MOVEGROUP,
        L"D\u00E9placer les naufrag\u00E9s avec le navire");
    y += rowH + 4;
    h = label(t, lx, y, tw, L"CHRONOLOGIE depuis la mise \u00E0 feu (Ctrl+F), en mm:ss");
    h->setOverrideColor(irr::video::SColor(255, 255, 200, 90));
    y += rowH;
    label(t, lx, y, lw, L"Abandon du navire \u00E0 T+");
    abandonBox = edit(t, fx, y, fw, ID_T_ABANDON);
    y += rowH;
    label(t, lx, y, lw, L"Feu g\u00E9n\u00E9ralis\u00E9 \u00E0 T+");
    spreadBox = edit(t, fx, y, fw, ID_T_SPREAD);
    y += rowH;
    label(t, lx, y, lw, L"Dur\u00E9e de l'incendie (coul\u00E9 \u00E0 T+)");
    durationBox = edit(t, fx, y, fw, ID_T_DURATION);
    y += rowH;
    label(t, lx, y, lw, L"Dur\u00E9e du naufrage (fin de l'incendie)");
    sinkLeadBox = edit(t, fx, y, fw, ID_T_SINKLEAD);
    y += rowH;
    label(t, lx, y, lw, L"G\u00EEte permanente si \u00E9teint apr\u00E8s T+");
    permListBox = edit(t, fx, y, fw, ID_T_PERMLIST);
    y += rowH;
    label(t, lx, y, lw, L"Intervalle entre naufrag\u00E9s (s)");
    intervalBox = edit(t, fx, y, fw, ID_T_INTERVAL);
    y += rowH + 4;
    // room for three rows of labels above the axis and three below
    irr::s32 timelineH = 6 * (irr::s32)font->getDimension(L"Ag").Height + 40;
    timelineArea = irr::core::recti(lx, y, lx + tw, y + timelineH);
    y += timelineH + 6;
    casInfoText = guienv->addStaticText(L"", irr::core::recti(lx, y, lx + tw, tabH), false, true, t);

    // --- Survivors ------------------------------------------------------------------------------
    t = tabSurvivors;
    y = 8;
    survList = guienv->addListBox(irr::core::recti(lx, y, lx + tw, y + rowH * 7), t, ID_SURV_LIST, true);
    y += rowH * 7 + 6;
    irr::s32 b3 = (tw - 8) / 3;
    button(t, lx, y, b3, ID_SURV_ADD_MOB, L"+ Homme \u00E0 la mer");
    button(t, lx + b3 + 4, y, b3, ID_SURV_ADD_LIFERAFT, L"+ Liferaft");
    button(t, lx + 2 * (b3 + 4), y, tw - 2 * (b3 + 4), ID_SURV_ADD_RADEAU, L"+ Radeau");
    y += rowH;
    button(t, lx, y, b3, ID_SURV_UP, L"Plus t\u00F4t");
    button(t, lx + b3 + 4, y, b3, ID_SURV_DOWN, L"Plus tard");
    button(t, lx + 2 * (b3 + 4), y, tw - 2 * (b3 + 4), ID_SURV_DELETE, L"Supprimer");
    y += rowH + 6;
    label(t, lx, y, 110, L"R\u00E9cup\u00E9r\u00E9 par");
    survBoatBox = combo(t, lx + 114, y, tw - 114, ID_SURV_BOAT);
    y += rowH + 4;
    button(t, lx, y, tw, ID_SURV_DEFAULT, L"Disposition par d\u00E9faut autour du navire en feu");
    y += rowH + 6;
    survInfoText = guienv->addStaticText(L"", irr::core::recti(lx, y, lx + tw, tabH), false, true, t);

    // --- SAR boats ------------------------------------------------------------------------------
    t = tabBoats;
    y = 8;
    boatList = guienv->addListBox(irr::core::recti(lx, y, lx + tw, y + rowH * 4), t, ID_BOAT_LIST, true);
    y += rowH * 4 + 6;
    irr::s32 b2 = (tw - 4) / 2;
    button(t, lx, y, b2, ID_BOAT_ADD, L"+ Ajouter une vedette");
    button(t, lx + b2 + 4, y, tw - b2 - 4, ID_BOAT_DELETE, L"Supprimer la vedette");
    y += rowH + 6;
    label(t, lx, y, 80, L"Mod\u00E8le");
    boatTypeBox = combo(t, lx + 84, y, tw - 84, ID_BOAT_TYPE);
    y += rowH;
    label(t, lx, y, lw, L"Cap au d\u00E9part (\u00B0)");
    boatHdgBox = edit(t, fx, y, fw, ID_BOAT_HDG);
    y += rowH;
    label(t, lx, y, lw, L"Vitesse (nds)");
    boatSpeedBox = edit(t, fx, y, fw, ID_BOAT_SPEED);
    y += rowH;
    label(t, lx, y, lw, L"Appareille apr\u00E8s les naufrag\u00E9s \u00E0 l'eau (mm:ss)");
    boatDelayBox = edit(t, fx, y, fw, ID_BOAT_DELAY);
    y += rowH;
    label(t, lx, y, lw, L"Cap \u00E0 quai (\u00B0, vide = cap de d\u00E9part)");
    boatMoorBox = edit(t, fx, y, fw, ID_BOAT_MOOR);
    y += rowH + 2;
    button(t, lx, y, tw, ID_BOAT_PLACE, L"Placer le point de d\u00E9part");
    y += rowH;
    irr::s32 bwide = tw - 104;
    button(t, lx, y, bwide, ID_BOAT_OUT, L"Tracer la route aller");
    button(t, lx + bwide + 4, y, 100, ID_BOAT_OUT_CLEAR, L"Effacer");
    y += rowH;
    button(t, lx, y, bwide, ID_BOAT_RET, L"Tracer la route retour");
    button(t, lx + bwide + 4, y, 100, ID_BOAT_RET_CLEAR, L"Effacer");
    y += rowH + 6;
    boatInfoText = guienv->addStaticText(L"", irr::core::recti(lx, y, lx + tw, tabH), false, true, t);

    // --- Helicopters ------------------------------------------------------------------------------
    t = tabHelos;
    y = 8;
    label(t, lx, y, lw, L"Sur zone apr\u00E8s l'appel OSC (3e Ctrl+A)");
    heloDelayBox = edit(t, fx, y, fw, ID_HELO_DELAY);
    y += rowH;
    label(t, lx, y, lw, L"Vitesse de transit (nds)");
    heloSpeedBox = edit(t, fx, y, fw, ID_HELO_SPEED);
    y += rowH + 6;
    heloList = guienv->addListBox(irr::core::recti(lx, y, lx + tw, y + rowH * 3), t, ID_HELO_LIST, true);
    y += rowH * 3 + 6;
    button(t, lx, y, b2, ID_HELO_ADD, L"+ Ajouter un h\u00E9lico");
    button(t, lx + b2 + 4, y, tw - b2 - 4, ID_HELO_DELETE, L"Supprimer l'h\u00E9lico");
    y += rowH + 6;
    label(t, lx, y, 80, L"Mod\u00E8le");
    heloModelBox = combo(t, lx + 84, y, tw - 84, ID_HELO_MODEL);
    y += rowH + 2;
    button(t, lx, y, bwide, ID_HELO_BASE, L"Placer la base de d\u00E9part");
    button(t, lx + bwide + 4, y, 100, ID_HELO_BASE_CLEAR, L"Effacer");
    y += rowH;
    label(t, lx, y, lw, L"Hauteur de la base (m)");
    heloBaseHBox = edit(t, fx, y, fw, ID_HELO_BASE_H);
    y += rowH;
    button(t, lx, y, bwide, ID_HELO_PAD, L"Placer l'h\u00E9lisurface de retour");
    button(t, lx + bwide + 4, y, 100, ID_HELO_PAD_CLEAR, L"Effacer");
    y += rowH;
    label(t, lx, y, lw, L"Hauteur de l'h\u00E9lisurface (m)");
    heloPadHBox = edit(t, fx, y, fw, ID_HELO_PAD_H);
    y += rowH + 6;
    heloInfoText = guienv->addStaticText(L"", irr::core::recti(lx, y, lx + tw, tabH), false, true, t);

    // Kyara: hover tooltip on every time field; clicking one shows the full explanation in the tab.
    irr::gui::IGUIEditBox* helpBoxes[] = { abandonBox, spreadBox, durationBox, sinkLeadBox, permListBox, intervalBox,
        boatDelayBox, heloDelayBox, heloSpeedBox };
    for (size_t i = 0; i < sizeof(helpBoxes) / sizeof(helpBoxes[0]); i++) {
        helpBoxes[i]->setToolTipText(FireHelp::tooltip(helpFieldFor(helpBoxes[i]->getID())).c_str());
    }

    if (!scn->hadIncidentFile) {
        statusText->setText(L"Ce sc\u00E9nario n'a pas encore de r\u00E9glages incendie : ce qui est affich\u00E9 correspond \u00E0 ce que fait le simulateur aujourd'hui. V\u00E9rifiez puis enregistrez.");
        statusText->setOverrideColor(irr::video::SColor(255, 255, 210, 120));
    }
}

// ------------------------------------------------------------------------------------------
// Keeping the panel in step

static void setEditText(irr::gui::IGUIEnvironment* env, irr::gui::IGUIEditBox* box, const std::wstring& text)
{
    if (env->getFocus() == box) { return; }   // do not fight the user's typing
    box->setText(text.c_str());
}

void FireEditor::refreshAll()
{
    refreshScenarioTab();
    refreshCasualtyTab();
    refreshSurvivorTab();
    refreshBoatTab();
    refreshHeloTab();
    refreshInfo();
    std::wstring caption = L"\u00C9diteur de sc\u00E9narios incendie / SAR - " + wide(scn->name) + (dirty ? L" *" : L"");
    device->setWindowCaption(caption.c_str());
}

void FireEditor::refreshScenarioTab()
{
    setEditText(guienv, dayBox, fmtNum(scn->day, 0));
    setEditText(guienv, monthBox, fmtNum(scn->month, 0));
    setEditText(guienv, yearBox, fmtNum(scn->year, 0));
    int minutes = (int)std::floor(scn->startTimeHours * 60.0f + 0.5f);
    wchar_t hm[16];
    swprintf(hm, 16, L"%02d:%02d", (minutes / 60) % 24, minutes % 60);
    setEditText(guienv, startBox, hm);
    setEditText(guienv, sunriseBox, fmtNum(scn->sunRise, 1));
    setEditText(guienv, sunsetBox, fmtNum(scn->sunSet, 1));
    setEditText(guienv, weatherBox, fmtNum(scn->weather, 1));
    setEditText(guienv, rainBox, fmtNum(scn->rain, 1));
    setEditText(guienv, visBox, fmtNum(scn->visibility, 1));
    setEditText(guienv, windDirBox, fmtNum(scn->windDirection, 0));
    setEditText(guienv, windSpdBox, fmtNum(scn->windSpeed, 1));
    setEditText(guienv, mrscBox, wide(scn->incident.coordinationCentre));
    setEditText(guienv, descBox, wide(scn->description));
    if (!guienv->hasFocus(ownTypeBox, true)) { fillTypeCombo(ownTypeBox, ownTypes, scn->ownShip.type); }
    setEditText(guienv, ownHdgBox, fmtNum(scn->ownShip.heading, 0));
    setEditText(guienv, ownSpdBox, fmtNum(scn->ownShip.speed, 1));
    wchar_t buf[256];
    swprintf(buf, 256, L"Autres navires du sc\u00E9nario : %d. Ils sont conserv\u00E9s tels quels (routes comprises) et peuvent \u00EAtre d\u00E9plac\u00E9s sur la carte.",
        (int)scn->traffic.size());
    trafficText->setText(buf);
}

void FireEditor::refreshCasualtyTab()
{
    if (!guienv->hasFocus(casTypeBox, true)) { fillTypeCombo(casTypeBox, otherTypes, scn->hasCasualty ? scn->casualty.type : ""); }
    setEditText(guienv, casHdgBox, fmtNum(scn->casualty.heading, 0));
    moveGroupBox->setChecked(moveGroupWithCasualty);
    const IncidentConfig& c = scn->incident;
    setEditText(guienv, abandonBox, fmtTime(c.abandonTime));
    setEditText(guienv, spreadBox, fmtTime(c.fireSpreadTime));
    setEditText(guienv, durationBox, fmtTime(c.fireDuration));
    setEditText(guienv, sinkLeadBox, fmtTime(c.sinkLeadTime));
    setEditText(guienv, permListBox, fmtTime(c.permanentListTime));
    setEditText(guienv, intervalBox, fmtNum(c.survivorInterval, 1));
}

std::wstring FireEditor::survivorName(int kind) const
{
    if (kind == Survivor_Liferaft) { return L"Liferaft"; }
    if (kind == Survivor_Radeau) { return L"Radeau de sauvetage"; }
    return L"Homme \u00E0 la mer";
}

std::wstring FireEditor::boatLabel(int index) const
{
    std::wstring s = L"Vedette " + fmtNum(index + 1, 0);
    if (index >= 0 && index < (int)scn->sarBoats.size()) { s += L" - " + wide(scn->sarBoats[index].type); }
    return s;
}

void FireEditor::refreshSurvivorTab()
{
    std::vector<IncidentSurvivor>& sv = scn->incident.survivors;
    if (selSurvivor >= (int)sv.size()) { selSurvivor = (int)sv.size() - 1; }
    survList->clear();
    for (size_t i = 0; i < sv.size(); i++) {
        std::wstring item = fmtNum((double)i + 1, 0) + L". " + survivorName(sv[i].kind);
        if (sv[i].kind == Survivor_MOB) { item += L"  (h\u00E9licos)"; }
        else if (sv[i].boat > 0) { item += L"  -> vedette " + fmtNum(sv[i].boat, 0); }
        else { item += L"  -> vedette la plus proche"; }
        survList->addItem(item.c_str());
    }
    survList->setSelected(selSurvivor);

    survBoatBox->clear();
    bool raft = selSurvivor >= 0 && sv[selSurvivor].kind != Survivor_MOB;
    if (selSurvivor >= 0 && !raft) {
        survBoatBox->addItem(L"H\u00E9licopt\u00E8res (h\u00E9litreuillage)");
        survBoatBox->setSelected(0);
    }
    else {
        survBoatBox->addItem(L"Vedette la plus proche");
        for (size_t b = 0; b < scn->sarBoats.size(); b++) { survBoatBox->addItem(boatLabel((int)b).c_str()); }
        int sel = (selSurvivor >= 0) ? sv[selSurvivor].boat : 0;
        if (sel < 0 || sel > (int)scn->sarBoats.size()) { sel = 0; }
        survBoatBox->setSelected(sel);
    }
    survBoatBox->setEnabled(raft);

    int mobs = 0, rafts = 0;
    for (size_t i = 0; i < sv.size(); i++) { if (sv[i].kind == Survivor_MOB) { mobs++; } else { rafts++; } }
    wchar_t buf[600];
    swprintf(buf, 600,
        L"%d homme(s) \u00E0 la mer, h\u00E9litreuill\u00E9s par les h\u00E9licopt\u00E8res. %d radeau(x), r\u00E9cup\u00E9r\u00E9s par les vedettes.\n\n"
        L"Choisissez un bouton + puis cliquez sur la carte (plusieurs clics possibles, clic droit pour terminer). "
        L"L'ordre de la liste est l'ordre de mise \u00E0 l'eau. Faites glisser un naufrag\u00E9 pour le d\u00E9placer.\n\n"
        L"Dans le simulateur, les naufrag\u00E9s restent plac\u00E9s par rapport au navire en feu : s'il a d\u00E9riv\u00E9, ils d\u00E9rivent avec lui.",
        mobs, rafts);
    survInfoText->setText(buf);
}

std::wstring FireEditor::rescueModelsLine() const
{
    wchar_t buf[256];
    swprintf(buf, 256, L"Mod\u00E8les propos\u00E9s : navires de Models/Othership avec FireFighting=1 dans leur boat.ini (%d install\u00E9(s)).\n\n",
        (int)rescueTypes.size());
    return buf;
}

void FireEditor::refreshBoatTab()
{
    if (selBoat >= (int)scn->sarBoats.size()) { selBoat = (int)scn->sarBoats.size() - 1; }
    boatList->clear();
    for (size_t b = 0; b < scn->sarBoats.size(); b++) { boatList->addItem(boatLabel((int)b).c_str()); }
    boatList->setSelected(selBoat);

    bool has = selBoat >= 0;
    boatTypeBox->setEnabled(has);
    boatHdgBox->setEnabled(has);
    boatSpeedBox->setEnabled(has);
    boatDelayBox->setEnabled(has);
    boatMoorBox->setEnabled(has);
    if (!has) {
        boatTypeBox->clear();
        boatHdgBox->setText(L""); boatSpeedBox->setText(L""); boatDelayBox->setText(L""); boatMoorBox->setText(L"");
        boatInfoText->setText((helpPrefix(TAB_BOATS) + rescueModelsLine() + L"Aucune vedette SAR : les radeaux ne seront pas r\u00E9cup\u00E9r\u00E9s. Ajoutez une vedette puis placez son point de d\u00E9part.").c_str());
        return;
    }
    const EdShip& s = scn->sarBoats[selBoat];
    const IncidentSarBoat& c = scn->incident.sarBoats[selBoat];
    if (!guienv->hasFocus(boatTypeBox, true)) { fillTypeCombo(boatTypeBox, rescueTypes, s.type); }
    setEditText(guienv, boatHdgBox, fmtNum(s.heading, 0));
    setEditText(guienv, boatSpeedBox, fmtNum(c.speedKts, 1));
    setEditText(guienv, boatDelayBox, fmtTime(c.launchDelay));
    setEditText(guienv, boatMoorBox, c.moorHeading < 0 ? std::wstring(L"") : fmtNum(c.moorHeading, 0));

    int assigned = 0, shared = 0;
    for (size_t i = 0; i < scn->incident.survivors.size(); i++) {
        const IncidentSurvivor& sv = scn->incident.survivors[i];
        if (sv.kind == Survivor_MOB) { continue; }
        if (sv.boat == selBoat + 1) { assigned++; }
        else if (sv.boat == 0) { shared++; }
    }
    wchar_t buf[800];
    swprintf(buf, 800,
        L"Route aller : %d point(s). Route retour : %d point(s)%ls.\n"
        L"Radeaux attribu\u00E9s : %d, plus %d radeau(x) pour la vedette la plus proche.\n\n"
        L"D\u00E9roulement : la vedette attend \u00E0 son point de d\u00E9part, appareille quand tous les naufrag\u00E9s sont \u00E0 l'eau "
        L"(plus le d\u00E9lai), suit la route aller, r\u00E9cup\u00E8re ses radeaux, puis suit la route retour et s'amarre au dernier point. "
        L"Sans route retour, elle revient \u00E0 son point de d\u00E9part par la route aller.\n\n"
        L"Tracer : cliquez les points sur la carte, clic droit pour terminer. Glissez un point pour le d\u00E9placer, Suppr pour l'effacer.",
        (int)c.outbound.size(), (int)c.inbound.size(),
        c.inbound.empty() ? L" (retour au d\u00E9part)" : L" (le dernier = poste \u00E0 quai)",
        assigned, shared);
    boatInfoText->setText((helpPrefix(TAB_BOATS) + rescueModelsLine() + buf).c_str());
}

void FireEditor::refreshHeloTab()
{
    IncidentConfig& c = scn->incident;
    setEditText(guienv, heloDelayBox, fmtTime(c.heloDelay));
    setEditText(guienv, heloSpeedBox, fmtNum(c.heloSpeedKts, 0));
    if (selHelo >= (int)c.helos.size()) { selHelo = (int)c.helos.size() - 1; }
    heloList->clear();
    for (size_t k = 0; k < c.helos.size(); k++) {
        std::wstring item = L"H\u00E9lico " + fmtNum((double)k + 1, 0) + L" - " + wide(c.helos[k].model);
        item += c.helos[k].hasBase ? L"  [base]" : L"  [sans base]";
        if (c.helos[k].hasPad) { item += L" [h\u00E9lisurface]"; }
        heloList->addItem(item.c_str());
    }
    heloList->setSelected(selHelo);
    bool has = selHelo >= 0;
    heloModelBox->setEnabled(has);
    heloBaseHBox->setEnabled(has);
    heloPadHBox->setEnabled(has);
    if (has) {
        if (!guienv->hasFocus(heloModelBox, true)) { fillTypeCombo(heloModelBox, otherTypes, c.helos[selHelo].model); }
        setEditText(guienv, heloBaseHBox, fmtNum(c.helos[selHelo].baseHeight, 0));
        setEditText(guienv, heloPadHBox, fmtNum(c.helos[selHelo].padHeight, 0));
    }
    else {
        heloModelBox->clear();
    }
    heloInfoText->setText((helpPrefix(TAB_HELOS) +
        L"Les h\u00E9licopt\u00E8res ne d\u00E9collent que si le stagiaire passe l'appel OSC (3e Ctrl+A). Ils arrivent sur zone le d\u00E9lai ci-dessus apr\u00E8s cet appel "
        L"(00:00 = imm\u00E9diatement, comme avant).\n\n"
        L"Avec une base : l'h\u00E9lico d\u00E9colle de sa base au bon moment et vole jusqu'\u00E0 la zone. Sans base : il appara\u00EEt sur zone \u00E0 l'heure pr\u00E9vue.\n"
        L"Apr\u00E8s l'op\u00E9ration il se pose sur son h\u00E9lisurface, sinon sur sa base, sinon il quitte la zone.\n\n"
        L"Les hauteurs sont en m\u00E8tres au-dessus de la flottaison du navire en feu.").c_str());
}

void FireEditor::refreshInfo()
{
    const IncidentConfig& c = scn->incident;
    int n = (int)c.survivors.size();
    float inWater = c.abandonTime + (n > 1 ? (float)(n - 1) * c.survivorInterval : 0.0f);
    std::wstring s;
    s += L"Naufrage : de T+" + fmtTime(c.sinkStartTime()) + L" \u00E0 T+" + fmtTime(c.fireDuration) + L" si le feu n'est pas ma\u00EEtris\u00E9.\n";
    s += L"Naufrag\u00E9s \u00E0 l'eau : de T+" + fmtTime(c.abandonTime) + L" \u00E0 T+" + fmtTime(inWater) + L".\n";
    for (size_t b = 0; b < c.sarBoats.size(); b++) {
        s += L"Vedette " + fmtNum((double)b + 1, 0) + L" appareille \u00E0 T+" + fmtTime(inWater + c.sarBoats[b].launchDelay) + L".\n";
    }
    s += L"H\u00E9licopt\u00E8res : sur zone " + fmtTime(c.heloDelay) + L" apr\u00E8s l'appel OSC du stagiaire.\n";

    int mobs = 0, rafts = 0;
    for (int i = 0; i < n; i++) { if (c.survivors[i].kind == Survivor_MOB) { mobs++; } else { rafts++; } }
    std::wstring warn;
    if (!scn->hasCasualty) { warn += L"- Aucun navire en feu : placez-le sur la carte.\n"; }
    if (c.sinkStartTime() <= c.abandonTime) { warn += L"- Le navire commence \u00E0 couler avant l'abandon.\n"; }
    else if (inWater > c.sinkStartTime()) { warn += L"- Le navire commence \u00E0 couler avant que tous les naufrag\u00E9s soient \u00E0 l'eau.\n"; }
    if (rafts > 0 && scn->sarBoats.empty()) { warn += L"- Des radeaux mais aucune vedette : ils ne seront pas r\u00E9cup\u00E9r\u00E9s.\n"; }
    if (mobs > 0 && c.helos.empty()) { warn += L"- Des hommes \u00E0 la mer mais aucun h\u00E9licopt\u00E8re.\n"; }
    for (size_t b = 0; b < scn->sarBoats.size(); b++) {
        const std::string& type = scn->sarBoats[b].type;
        if (type.empty()) { warn += L"- Vedette " + fmtNum((double)b + 1, 0) + L" sans mod\u00E8le.\n"; }
        else if (std::find(rescueTypes.begin(), rescueTypes.end(), type) == rescueTypes.end()) {
            warn += L"- Vedette " + fmtNum((double)b + 1, 0) + L" : le mod\u00E8le " + wide(type) + L" n'a pas FireFighting=1 dans son boat.ini.\n";
        }
    }
    if (!warn.empty()) { s += L"\nA V\u00C9RIFIER :\n" + warn; }
    casInfoText->setText((helpPrefix(TAB_CASUALTY) + s).c_str());
}

FireHelp::Field FireEditor::helpFieldFor(irr::s32 id) const
{
    switch (id) {
    case ID_T_ABANDON:  return FireHelp::Field_AbandonTime;
    case ID_T_SPREAD:   return FireHelp::Field_FireSpreadTime;
    case ID_T_DURATION: return FireHelp::Field_FireDuration;
    case ID_T_SINKLEAD: return FireHelp::Field_SinkLeadTime;
    case ID_T_PERMLIST: return FireHelp::Field_PermanentListTime;
    case ID_T_INTERVAL: return FireHelp::Field_SurvivorInterval;
    case ID_BOAT_DELAY: return FireHelp::Field_SarBoatDelay;
    case ID_HELO_DELAY: return FireHelp::Field_HeloDelay;
    case ID_HELO_SPEED: return FireHelp::Field_HeloSpeed;
    default:            return FireHelp::Field_None;
    }
}

std::wstring FireEditor::helpPrefix(irr::s32 tab) const
{
    irr::s32 home = -1;
    switch (helpField) {
    case FireHelp::Field_SarBoatDelay: home = TAB_BOATS; break;
    case FireHelp::Field_HeloDelay: case FireHelp::Field_HeloSpeed: home = TAB_HELOS; break;
    case FireHelp::Field_None: break;
    default: home = TAB_CASUALTY; break;
    }
    if (home != tab) { return L""; }
    return FireHelp::title(helpField) + L"\n" + FireHelp::explanation(helpField) + L"\n\n";
}

bool FireEditor::timelineProblem(std::wstring& message) const
{
    const IncidentConfig& c = scn->incident;
    if (!FireHelp::hasBlockingProblem(c)) { return false; }
    int n = (int)c.survivors.size();
    float inWater = c.abandonTime + (n > 1 ? (float)(n - 1) * c.survivorInterval : 0.0f);
    message = L"Le navire commence \u00E0 couler \u00E0 T+" + fmtTime(c.sinkStartTime())
        + L" (dur\u00E9e de l'incendie " + fmtTime(c.fireDuration) + L" - dur\u00E9e du naufrage " + fmtTime(c.sinkLeadTime) + L"), "
        + L"alors que l'abandon est \u00E0 T+" + fmtTime(c.abandonTime)
        + (n > 0 ? L" et le dernier naufrag\u00E9 \u00E0 l'eau \u00E0 T+" + fmtTime(inWater) : std::wstring(L"")) + L". "
        + L"L'exercice sera perdu \u00E0 T+" + fmtTime(c.sinkStartTime()) + L" si le feu n'est pas \u00E9teint.";
    return true;
}

void FireEditor::setTool(Tool t)
{
    tool = t;
    dragging = false;
}

void FireEditor::select(const Pick& p, bool switchTab)
{
    selected = p;
    switch (p.type) {
    case Pick_Own: if (switchTab) { tabs->setActiveTab(TAB_SCENARIO); } break;
    case Pick_Casualty: if (switchTab) { tabs->setActiveTab(TAB_CASUALTY); } break;
    case Pick_Boat: case Pick_Out: case Pick_Ret:
        selBoat = p.index;
        if (switchTab) { tabs->setActiveTab(TAB_BOATS); }
        break;
    case Pick_Survivor:
        selSurvivor = p.index;
        if (switchTab) { tabs->setActiveTab(TAB_SURVIVORS); }
        break;
    case Pick_HeloBase: case Pick_HeloPad:
        selHelo = p.index;
        if (switchTab) { tabs->setActiveTab(TAB_HELOS); }
        break;
    default: break;
    }
    refreshAll();
}

void FireEditor::markDirty()
{
    dirty = true;
}

// ------------------------------------------------------------------------------------------
// Edits

bool FireEditor::OnEvent(const irr::SEvent& event)
{
    if (event.EventType == irr::EET_GUI_EVENT) {
        return onGuiEvent(event.GUIEvent);
    }

    if (event.EventType == irr::EET_KEY_INPUT_EVENT && event.KeyInput.PressedDown) {
        irr::gui::IGUIElement* focus = guienv->getFocus();
        bool typing = focus && (focus->getType() == irr::gui::EGUIET_EDIT_BOX);
        if (typing) {
            if (event.KeyInput.Key == irr::KEY_ESCAPE) { guienv->setFocus(0); refreshAll(); return true; }
            return false;
        }
        switch (event.KeyInput.Key) {
        case irr::KEY_DELETE:
        case irr::KEY_BACK:
            deleteSelected();
            return true;
        case irr::KEY_ESCAPE:
            setTool(Tool_Select);
            return true;
        case irr::KEY_HOME:
            map->centreOn(scn->hasCasualty ? scn->casualty.pos : scn->ownShip.pos);
            return true;
        case irr::KEY_LEFT:
        case irr::KEY_RIGHT: {
            float* hdg = headingOf(selected);
            if (!hdg) { return false; }
            float step = event.KeyInput.Shift ? 1.0f : 5.0f;
            *hdg += (event.KeyInput.Key == irr::KEY_LEFT) ? -step : step;
            while (*hdg < 0.0f) { *hdg += 360.0f; }
            while (*hdg >= 360.0f) { *hdg -= 360.0f; }
            markDirty();
            refreshAll();
            return true;
        }
        default:
            return false;
        }
    }

    if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
        irr::core::position2di at(event.MouseInput.X, event.MouseInput.Y);
        mouse = at;
        switch (event.MouseInput.Event) {
        case irr::EMIE_MOUSE_MOVED: {
            if (dragging) {
                moveObject(selected, map->toLatLong(at + dragOffset));
                markDirty();
            }
            else if (panning) {
                map->panPixels(at.X - lastMouse.X, at.Y - lastMouse.Y);
            }
            if (rightDown && (std::abs(at.X - rightDownAt.X) > 3 || std::abs(at.Y - rightDownAt.Y) > 3)) { rightMoved = true; }
            lastMouse = at;
            return dragging || panning;
        }
        case irr::EMIE_MOUSE_WHEEL:
            if (!overMap(at)) { return false; }
            map->zoomAt(at, event.MouseInput.Wheel > 0 ? 0.8f : 1.25f);
            return true;
        case irr::EMIE_LMOUSE_PRESSED_DOWN: {
            if (!overMap(at)) { return false; }
            guienv->setFocus(0);
            leftDown = true;
            lastMouse = at;
            if (tool != Tool_Select) {
                clickMap(at);
                return true;
            }
            Pick p = pickAt(at);
            if (p.type != Pick_None) {
                select(p, true);
                IncidentPoint* pos = positionOf(p);
                dragging = (pos != 0);
                if (pos) { dragOffset = map->toScreen(*pos) - at; }
            }
            else {
                select(Pick(), false);
                panning = true;
            }
            return true;
        }
        case irr::EMIE_LMOUSE_LEFT_UP: {
            bool wasDragging = dragging;
            leftDown = false;
            dragging = false;
            panning = rightDown;
            if (wasDragging) { refreshAll(); }
            return false;
        }
        case irr::EMIE_RMOUSE_PRESSED_DOWN:
        case irr::EMIE_MMOUSE_PRESSED_DOWN:
            if (!overMap(at)) { return false; }
            rightDown = true;
            rightMoved = false;
            rightDownAt = at;
            lastMouse = at;
            panning = true;
            return true;
        case irr::EMIE_RMOUSE_LEFT_UP:
        case irr::EMIE_MMOUSE_LEFT_UP:
            if (rightDown && !rightMoved && tool != Tool_Select) { setTool(Tool_Select); }
            rightDown = false;
            panning = false;
            return false;
        default:
            return false;
        }
    }
    return false;
}

bool FireEditor::onGuiEvent(const irr::SEvent::SGUIEvent& e)
{
    irr::s32 id = e.Caller ? e.Caller->getID() : -1;
    switch (e.EventType) {
    case irr::gui::EGET_BUTTON_CLICKED:
        onButton(id);
        return true;
    case irr::gui::EGET_EDITBOX_CHANGED:
    case irr::gui::EGET_EDITBOX_ENTER:
        onEditChanged(id, e.Caller->getText());
        if (e.EventType == irr::gui::EGET_EDITBOX_ENTER && id != ID_DESC) { guienv->setFocus(0); refreshAll(); }
        return false;
    case irr::gui::EGET_ELEMENT_FOCUSED:
        // Kyara: a time field was clicked - explain it in its tab's info text.
        if (helpFieldFor(id) != FireHelp::Field_None) {
            helpField = helpFieldFor(id);
            needRefresh = true;
        }
        return false;
    case irr::gui::EGET_ELEMENT_FOCUS_LOST:
        // Show the value as it was understood once the user leaves the box (next frame, once
        // focus has moved on).
        if (e.Caller && e.Caller->getType() == irr::gui::EGUIET_EDIT_BOX) { needRefresh = true; }
        if (helpFieldFor(id) == helpField) { helpField = FireHelp::Field_None; }   // FOCUSED re-sets it if another field took over
        return false;
    case irr::gui::EGET_COMBO_BOX_CHANGED:
        onComboChanged(id, (irr::gui::IGUIComboBox*)e.Caller);
        return false;
    case irr::gui::EGET_LISTBOX_CHANGED:
    case irr::gui::EGET_LISTBOX_SELECTED_AGAIN: {
        irr::s32 sel = ((irr::gui::IGUIListBox*)e.Caller)->getSelected();
        if (id == ID_SURV_LIST && sel >= 0) { select(Pick(Pick_Survivor, sel), false); }
        if (id == ID_BOAT_LIST && sel >= 0) { select(Pick(Pick_Boat, sel), false); }
        if (id == ID_HELO_LIST && sel >= 0) {
            selHelo = sel;
            const IncidentHelo& h = scn->incident.helos[sel];
            select(h.hasBase ? Pick(Pick_HeloBase, sel) : (h.hasPad ? Pick(Pick_HeloPad, sel) : Pick()), false);
            selHelo = sel;
            refreshAll();
        }
        return false;
    }
    case irr::gui::EGET_CHECKBOX_CHANGED:
        if (id == ID_CAS_MOVEGROUP) { moveGroupWithCasualty = ((irr::gui::IGUICheckBox*)e.Caller)->isChecked(); }
        return false;
    case irr::gui::EGET_MESSAGEBOX_YES:
        if (id == ID_CONFIRM_OVERWRITE) { save(true); }
        if (id == ID_CONFIRM_MENU) { backToMenu = true; }
        if (id == ID_CONFIRM_TIMELINE) { timelineConfirmed = true; save(false); timelineConfirmed = false; }
        return false;
    default:
        return false;
    }
}

void FireEditor::onEditChanged(irr::s32 id, const std::wstring& text)
{
    double v = 0;
    float t = 0;
    bool num = parseNumber(text, v);
    IncidentConfig& c = scn->incident;
    bool changed = true;
    switch (id) {
    case ID_NAME: scn->name = narrow(trimW(text)); break;
    case ID_DAY: if (num && v >= 1 && v <= 31) { scn->day = (unsigned int)v; } else { changed = false; } break;
    case ID_MONTH: if (num && v >= 1 && v <= 12) { scn->month = (unsigned int)v; } else { changed = false; } break;
    case ID_YEAR: if (num && v >= 1970 && v <= 2100) { scn->year = (unsigned int)v; } else { changed = false; } break;
    case ID_START:
        if (parseTime(text, t) && t < 24 * 60) { scn->startTimeHours = t / 60.0f; } else { changed = false; }   // hh:mm, read as mm:ss
        break;
    case ID_SUNRISE: if (num && v >= 0 && v < 24) { scn->sunRise = (float)v; } else { changed = false; } break;
    case ID_SUNSET: if (num && v >= 0 && v < 24) { scn->sunSet = (float)v; } else { changed = false; } break;
    case ID_WEATHER: if (num && v >= 0 && v <= 12) { scn->weather = (float)v; } else { changed = false; } break;
    case ID_RAIN: if (num && v >= 0 && v <= 10) { scn->rain = (float)v; } else { changed = false; } break;
    case ID_VIS: if (num && v > 0) { scn->visibility = (float)v; } else { changed = false; } break;
    case ID_WINDDIR: if (num && v >= 0 && v <= 360) { scn->windDirection = (float)v; } else { changed = false; } break;
    case ID_WINDSPD: if (num && v >= 0) { scn->windSpeed = (float)v; } else { changed = false; } break;
    case ID_DESC: scn->description = narrow(text); break;
    case ID_MRSC: scn->incident.coordinationCentre = narrow(trimW(text)); break;
    case ID_OWN_HDG: if (num) { scn->ownShip.heading = (float)std::fmod(v + 360.0, 360.0); } else { changed = false; } break;
    case ID_OWN_SPD: if (num) { scn->ownShip.speed = (float)v; } else { changed = false; } break;
    case ID_CAS_HDG: if (num) { scn->casualty.heading = (float)std::fmod(v + 360.0, 360.0); } else { changed = false; } break;
    case ID_T_ABANDON: if (parseTime(text, t)) { c.abandonTime = t; } else { changed = false; } break;
    case ID_T_SPREAD: if (parseTime(text, t) && t >= 1) { c.fireSpreadTime = t; } else { changed = false; } break;
    case ID_T_DURATION: if (parseTime(text, t) && t >= 1) { c.fireDuration = t; } else { changed = false; } break;
    case ID_T_SINKLEAD: if (parseTime(text, t) && t >= 1) { c.sinkLeadTime = t; } else { changed = false; } break;
    case ID_T_PERMLIST: if (parseTime(text, t)) { c.permanentListTime = t; } else { changed = false; } break;
    case ID_T_INTERVAL: if (num && v >= 0) { c.survivorInterval = (float)v; } else { changed = false; } break;
    case ID_BOAT_HDG:
        if (num && selBoat >= 0) { scn->sarBoats[selBoat].heading = (float)std::fmod(v + 360.0, 360.0); } else { changed = false; }
        break;
    case ID_BOAT_SPEED: if (num && v >= 1 && selBoat >= 0) { c.sarBoats[selBoat].speedKts = (float)v; } else { changed = false; } break;
    case ID_BOAT_DELAY: if (parseTime(text, t) && selBoat >= 0) { c.sarBoats[selBoat].launchDelay = t; } else { changed = false; } break;
    case ID_BOAT_MOOR:
        if (selBoat < 0) { changed = false; }
        else if (trimW(text).empty()) { c.sarBoats[selBoat].moorHeading = -1.0f; }
        else if (num) { c.sarBoats[selBoat].moorHeading = (float)std::fmod(v + 360.0, 360.0); }
        else { changed = false; }
        break;
    case ID_HELO_DELAY: if (parseTime(text, t)) { c.heloDelay = t; } else { changed = false; } break;
    case ID_HELO_SPEED: if (num && v >= 1) { c.heloSpeedKts = (float)v; } else { changed = false; } break;
    case ID_HELO_BASE_H: if (num && selHelo >= 0) { c.helos[selHelo].baseHeight = (float)v; } else { changed = false; } break;
    case ID_HELO_PAD_H: if (num && selHelo >= 0) { c.helos[selHelo].padHeight = (float)v; } else { changed = false; } break;
    default: changed = false; break;
    }
    if (changed) {
        markDirty();
        refreshInfo();
        std::wstring caption = L"\u00C9diteur de sc\u00E9narios incendie / SAR - " + wide(scn->name) + L" *";
        device->setWindowCaption(caption.c_str());
    }
}

void FireEditor::onComboChanged(irr::s32 id, irr::gui::IGUIComboBox* box)
{
    irr::s32 sel = box->getSelected();
    if (sel < 0) { return; }
    std::string text = narrow(box->getItem(sel));
    switch (id) {
    case ID_OWN_TYPE: scn->ownShip.type = text; break;
    case ID_CAS_TYPE: scn->casualty.type = text; break;
    case ID_BOAT_TYPE: if (selBoat >= 0) { scn->sarBoats[selBoat].type = text; } break;
    case ID_HELO_MODEL: if (selHelo >= 0) { scn->incident.helos[selHelo].model = text; } break;
    case ID_SURV_BOAT:
        if (selSurvivor >= 0 && scn->incident.survivors[selSurvivor].kind != Survivor_MOB) {
            scn->incident.survivors[selSurvivor].boat = sel;   // 0 = nearest, n = boat n
        }
        break;
    default: return;
    }
    markDirty();
    guienv->setFocus(0);
    refreshAll();
}

void FireEditor::onButton(irr::s32 id)
{
    IncidentConfig& c = scn->incident;
    switch (id) {
    case ID_SAVE: save(false); return;
    case ID_STYLE:
        map->nextStyle();
        styleButton->setText((L"Fond : " + map->styleName()).c_str());
        guienv->setFocus(0);
        return;
    case ID_MENU:
        if (dirty) {
            guienv->addMessageBox(L"Modifications non enregistr\u00E9es",
                L"Revenir au menu sans enregistrer les modifications ?", true,
                irr::gui::EMBF_YES | irr::gui::EMBF_NO, 0, ID_CONFIRM_MENU);
        }
        else { backToMenu = true; }
        return;
    case ID_OWN_PLACE: setTool(Tool_PlaceOwn); break;
    case ID_CAS_PLACE: setTool(Tool_PlaceCasualty); break;
    case ID_SURV_ADD_MOB: setTool(Tool_AddMob); break;
    case ID_SURV_ADD_LIFERAFT: setTool(Tool_AddLiferaft); break;
    case ID_SURV_ADD_RADEAU: setTool(Tool_AddRadeau); break;
    case ID_SURV_DELETE:
        if (selSurvivor >= 0) { select(Pick(Pick_Survivor, selSurvivor), false); deleteSelected(); }
        break;
    case ID_SURV_UP:
    case ID_SURV_DOWN: {
        int other = selSurvivor + (id == ID_SURV_UP ? -1 : 1);
        if (selSurvivor >= 0 && other >= 0 && other < (int)c.survivors.size()) {
            std::swap(c.survivors[selSurvivor], c.survivors[other]);
            selSurvivor = other;
            selected = Pick(Pick_Survivor, other);
            markDirty();
        }
        break;
    }
    case ID_SURV_DEFAULT:
        if (scn->hasCasualty) {
            c.survivors = FireScenario::defaultSurvivors(scn->casualty.pos);
            selSurvivor = -1;
            selected = Pick();
            markDirty();
        }
        break;
    case ID_BOAT_ADD: {
        if (rescueTypes.empty()) {
            statusText->setText(L"Aucun mod\u00E8le de vedette disponible : seuls les navires de Models/Othership dont le boat.ini contient FireFighting=1 peuvent \u00EAtre des vedettes SAR.");
            statusText->setOverrideColor(irr::video::SColor(255, 255, 110, 110));
            return;
        }
        EdShip s;
        s.type = rescueTypes[0];
        if (!scn->sarBoats.empty() && std::find(rescueTypes.begin(), rescueTypes.end(), scn->sarBoats.back().type) != rescueTypes.end()) {
            s.type = scn->sarBoats.back().type;
        }
        s.pos = map->centre();
        s.mmsi = scn->nextMmsi();
        scn->sarBoats.push_back(s);
        IncidentSarBoat cfg;
        if (!c.sarBoats.empty()) { cfg.speedKts = c.sarBoats.back().speedKts; }
        c.sarBoats.push_back(cfg);
        selBoat = (int)scn->sarBoats.size() - 1;
        selected = Pick(Pick_Boat, selBoat);
        setTool(Tool_PlaceBoat);
        markDirty();
        break;
    }
    case ID_BOAT_DELETE: if (selBoat >= 0) { removeBoat(selBoat); markDirty(); } break;
    case ID_BOAT_PLACE: if (selBoat >= 0) { setTool(Tool_PlaceBoat); } break;
    case ID_BOAT_OUT: if (selBoat >= 0) { setTool(Tool_AddOut); } break;
    case ID_BOAT_RET: if (selBoat >= 0) { setTool(Tool_AddRet); } break;
    case ID_BOAT_OUT_CLEAR: if (selBoat >= 0) { c.sarBoats[selBoat].outbound.clear(); selected = Pick(); markDirty(); } break;
    case ID_BOAT_RET_CLEAR: if (selBoat >= 0) { c.sarBoats[selBoat].inbound.clear(); selected = Pick(); markDirty(); } break;
    case ID_HELO_ADD: {
        IncidentHelo hl;
        if (!c.helos.empty()) { hl.model = c.helos.back().model; }
        c.helos.push_back(hl);
        selHelo = (int)c.helos.size() - 1;
        markDirty();
        break;
    }
    case ID_HELO_DELETE:
        if (selHelo >= 0) {
            c.helos.erase(c.helos.begin() + selHelo);
            selected = Pick();
            markDirty();
        }
        break;
    case ID_HELO_BASE: if (selHelo >= 0) { setTool(Tool_PlaceHeloBase); } break;
    case ID_HELO_PAD: if (selHelo >= 0) { setTool(Tool_PlaceHeloPad); } break;
    case ID_HELO_BASE_CLEAR: if (selHelo >= 0) { c.helos[selHelo].hasBase = false; selected = Pick(); markDirty(); } break;
    case ID_HELO_PAD_CLEAR: if (selHelo >= 0) { c.helos[selHelo].hasPad = false; selected = Pick(); markDirty(); } break;
    default: return;
    }
    refreshAll();
}

void FireEditor::clickMap(irr::core::position2di at)
{
    IncidentPoint p = map->toLatLong(at);
    IncidentConfig& c = scn->incident;
    switch (tool) {
    case Tool_PlaceOwn:
        scn->ownShip.pos = p;
        selected = Pick(Pick_Own, 0);
        setTool(Tool_Select);
        break;
    case Tool_PlaceCasualty:
        if (!scn->hasCasualty) {
            scn->hasCasualty = true;
            scn->casualty.pos = p;
            if (scn->casualty.type.empty() && !otherTypes.empty()) { scn->casualty.type = otherTypes[0]; }
            if (scn->casualty.mmsi == 0) { scn->casualty.mmsi = scn->nextMmsi(); }
        }
        moveObject(Pick(Pick_Casualty, 0), p);
        selected = Pick(Pick_Casualty, 0);
        setTool(Tool_Select);
        break;
    case Tool_PlaceBoat:
        if (selBoat >= 0) { scn->sarBoats[selBoat].pos = p; selected = Pick(Pick_Boat, selBoat); }
        setTool(Tool_Select);
        break;
    case Tool_AddOut:
        if (selBoat >= 0) {
            c.sarBoats[selBoat].outbound.push_back(p);
            selected = Pick(Pick_Out, selBoat, (int)c.sarBoats[selBoat].outbound.size() - 1);
        }
        break;
    case Tool_AddRet:
        if (selBoat >= 0) {
            c.sarBoats[selBoat].inbound.push_back(p);
            selected = Pick(Pick_Ret, selBoat, (int)c.sarBoats[selBoat].inbound.size() - 1);
        }
        break;
    case Tool_AddMob:
    case Tool_AddLiferaft:
    case Tool_AddRadeau: {
        IncidentSurvivor s;
        s.kind = (tool == Tool_AddMob) ? Survivor_MOB : (tool == Tool_AddLiferaft ? Survivor_Liferaft : Survivor_Radeau);
        s.pos = p;
        c.survivors.push_back(s);
        selSurvivor = (int)c.survivors.size() - 1;
        selected = Pick(Pick_Survivor, selSurvivor);
        break;
    }
    case Tool_PlaceHeloBase:
        if (selHelo >= 0) { c.helos[selHelo].hasBase = true; c.helos[selHelo].base = p; selected = Pick(Pick_HeloBase, selHelo); }
        setTool(Tool_Select);
        break;
    case Tool_PlaceHeloPad:
        if (selHelo >= 0) { c.helos[selHelo].hasPad = true; c.helos[selHelo].pad = p; selected = Pick(Pick_HeloPad, selHelo); }
        setTool(Tool_Select);
        break;
    default:
        return;
    }
    markDirty();
    refreshAll();
}

IncidentPoint* FireEditor::positionOf(const Pick& p)
{
    IncidentConfig& c = scn->incident;
    switch (p.type) {
    case Pick_Own: return &scn->ownShip.pos;
    case Pick_Casualty: return scn->hasCasualty ? &scn->casualty.pos : 0;
    case Pick_Boat: return (p.index >= 0 && p.index < (int)scn->sarBoats.size()) ? &scn->sarBoats[p.index].pos : 0;
    case Pick_Out:
        if (p.index < 0 || p.index >= (int)c.sarBoats.size()) { return 0; }
        return (p.sub >= 0 && p.sub < (int)c.sarBoats[p.index].outbound.size()) ? &c.sarBoats[p.index].outbound[p.sub] : 0;
    case Pick_Ret:
        if (p.index < 0 || p.index >= (int)c.sarBoats.size()) { return 0; }
        return (p.sub >= 0 && p.sub < (int)c.sarBoats[p.index].inbound.size()) ? &c.sarBoats[p.index].inbound[p.sub] : 0;
    case Pick_Survivor: return (p.index >= 0 && p.index < (int)c.survivors.size()) ? &c.survivors[p.index].pos : 0;
    case Pick_HeloBase: return (p.index >= 0 && p.index < (int)c.helos.size() && c.helos[p.index].hasBase) ? &c.helos[p.index].base : 0;
    case Pick_HeloPad: return (p.index >= 0 && p.index < (int)c.helos.size() && c.helos[p.index].hasPad) ? &c.helos[p.index].pad : 0;
    case Pick_Traffic: return (p.index >= 0 && p.index < (int)scn->traffic.size()) ? &scn->traffic[p.index].pos : 0;
    default: return 0;
    }
}

float* FireEditor::headingOf(const Pick& p)
{
    switch (p.type) {
    case Pick_Own: return &scn->ownShip.heading;
    case Pick_Casualty: return scn->hasCasualty ? &scn->casualty.heading : 0;
    case Pick_Boat: return (p.index >= 0 && p.index < (int)scn->sarBoats.size()) ? &scn->sarBoats[p.index].heading : 0;
    default: return 0;
    }
}

void FireEditor::moveObject(const Pick& p, const IncidentPoint& to)
{
    IncidentPoint* pos = positionOf(p);
    if (!pos) { return; }
    if (p.type == Pick_Casualty && moveGroupWithCasualty) {
        double dLat = to.lat - pos->lat, dLon = to.lon - pos->lon;
        for (size_t i = 0; i < scn->incident.survivors.size(); i++) {
            scn->incident.survivors[i].pos.lat += dLat;
            scn->incident.survivors[i].pos.lon += dLon;
        }
    }
    *pos = to;
}

void FireEditor::removeBoat(int index)
{
    if (index < 0 || index >= (int)scn->sarBoats.size()) { return; }
    scn->sarBoats.erase(scn->sarBoats.begin() + index);
    scn->incident.sarBoats.erase(scn->incident.sarBoats.begin() + index);
    for (size_t i = 0; i < scn->incident.survivors.size(); i++) {
        int& b = scn->incident.survivors[i].boat;
        if (b == index + 1) { b = 0; }
        else if (b > index + 1) { b--; }
    }
    if (selBoat >= (int)scn->sarBoats.size()) { selBoat = (int)scn->sarBoats.size() - 1; }
    selected = Pick();
}

void FireEditor::deleteSelected()
{
    IncidentConfig& c = scn->incident;
    Pick p = selected;
    switch (p.type) {
    case Pick_Out: if (positionOf(p)) { c.sarBoats[p.index].outbound.erase(c.sarBoats[p.index].outbound.begin() + p.sub); } break;
    case Pick_Ret: if (positionOf(p)) { c.sarBoats[p.index].inbound.erase(c.sarBoats[p.index].inbound.begin() + p.sub); } break;
    case Pick_Survivor:
        if (positionOf(p)) {
            c.survivors.erase(c.survivors.begin() + p.index);
            if (selSurvivor >= (int)c.survivors.size()) { selSurvivor = (int)c.survivors.size() - 1; }
        }
        break;
    case Pick_HeloBase: if (positionOf(p)) { c.helos[p.index].hasBase = false; } break;
    case Pick_HeloPad: if (positionOf(p)) { c.helos[p.index].hasPad = false; } break;
    case Pick_Boat: removeBoat(p.index); break;
    case Pick_Traffic: if (positionOf(p)) { scn->traffic.erase(scn->traffic.begin() + p.index); } break;
    default: return;   // own ship and the casualty stay
    }
    selected = Pick();
    markDirty();
    refreshAll();
}

bool FireEditor::save(bool confirmedOverwrite)
{
    std::string name = narrow(trimW(nameBox->getText()));
    std::string bad = "/\\:*?\"<>|";
    if (name.empty() || name.find_first_of(bad) != std::string::npos) {
        statusText->setText(L"Nom invalide : il ne doit pas \u00EAtre vide ni contenir / \\ : * ? \" < > |");
        statusText->setOverrideColor(irr::video::SColor(255, 255, 110, 110));
        return false;
    }
    // Kyara: do not silently save a timeline the trainee cannot win (e.g. sinking before the
    // survivors are in the water) - the instructor can still confirm.
    std::wstring problem;
    if (!confirmedOverwrite && !timelineConfirmed && timelineProblem(problem)) {
        guienv->addMessageBox(L"Chronologie \u00E0 v\u00E9rifier", (problem + L"\n\nEnregistrer quand m\u00EAme ?").c_str(),
            true, irr::gui::EMBF_YES | irr::gui::EMBF_NO, 0, ID_CONFIRM_TIMELINE);
        tabs->setActiveTab(TAB_CASUALTY);
        return false;
    }
    std::string dir = scenariosPath + name;
    if (!confirmedOverwrite && name != loadedName && Utilities::pathExists(dir)) {
        std::wstring text = L"Un sc\u00E9nario \"" + wide(name) + L"\" existe d\u00E9j\u00E0. Le remplacer ?";
        guienv->addMessageBox(L"Remplacer ?", text.c_str(), true, irr::gui::EMBF_YES | irr::gui::EMBF_NO, 0, ID_CONFIRM_OVERWRITE);
        return false;
    }
    scn->name = name;
    std::string error;
    if (!scn->save(dir, error)) {
        statusText->setText(wide(error).c_str());
        statusText->setOverrideColor(irr::video::SColor(255, 255, 110, 110));
        return false;
    }
    loadedName = name;
    dirty = false;
    statusText->setText((L"Enregistr\u00E9 : " + wide(dir)).c_str());
    statusText->setOverrideColor(irr::video::SColor(255, 120, 230, 120));
    refreshAll();
    return true;
}

// ------------------------------------------------------------------------------------------
// Map: picking and drawing

bool FireEditor::overMap(irr::core::position2di at) const
{
    if (!map->getViewport().isPointInside(at)) { return false; }
    irr::gui::IGUIElement* root = guienv->getRootGUIElement();
    irr::gui::IGUIElement* over = root->getElementFromPoint(at);
    return over == 0 || over == root;
}

FireEditor::Pick FireEditor::pickAt(irr::core::position2di at)
{
    Pick best;
    irr::s32 bestD = 12 * 12;
    IncidentConfig& c = scn->incident;
    // Later candidates win ties, so the small markers drawn on top are easiest to grab.
    std::vector<Pick> candidates;
    for (size_t i = 0; i < scn->traffic.size(); i++) { candidates.push_back(Pick(Pick_Traffic, (int)i)); }
    candidates.push_back(Pick(Pick_Own, 0));
    if (scn->hasCasualty) { candidates.push_back(Pick(Pick_Casualty, 0)); }
    for (size_t b = 0; b < scn->sarBoats.size(); b++) { candidates.push_back(Pick(Pick_Boat, (int)b)); }
    for (size_t k = 0; k < c.helos.size(); k++) {
        candidates.push_back(Pick(Pick_HeloBase, (int)k));
        candidates.push_back(Pick(Pick_HeloPad, (int)k));
    }
    for (size_t i = 0; i < c.survivors.size(); i++) { candidates.push_back(Pick(Pick_Survivor, (int)i)); }
    for (size_t b = 0; b < c.sarBoats.size(); b++) {
        for (size_t w = 0; w < c.sarBoats[b].outbound.size(); w++) { candidates.push_back(Pick(Pick_Out, (int)b, (int)w)); }
        for (size_t w = 0; w < c.sarBoats[b].inbound.size(); w++) { candidates.push_back(Pick(Pick_Ret, (int)b, (int)w)); }
    }
    for (size_t i = 0; i < candidates.size(); i++) {
        IncidentPoint* pos = positionOf(candidates[i]);
        if (!pos) { continue; }
        irr::core::position2di s = map->toScreen(*pos);
        irr::s32 dx = s.X - at.X, dy = s.Y - at.Y;
        irr::s32 d = dx * dx + dy * dy;
        if (d <= bestD) { bestD = d; best = candidates[i]; }
    }
    return best;
}

irr::video::SColor FireEditor::boatColour(int index) const
{
    static const irr::video::SColor palette[6] = {
        irr::video::SColor(255, 255, 140, 0), irr::video::SColor(255, 40, 185, 70), irr::video::SColor(255, 0, 165, 225),
        irr::video::SColor(255, 225, 70, 210), irr::video::SColor(255, 205, 85, 40), irr::video::SColor(255, 130, 95, 255) };
    return palette[(index < 0 ? 0 : index) % 6];
}

void FireEditor::fillPolygon(const std::vector<irr::core::position2df>& pts, irr::video::SColor color)
{
    ChartDraw::fillPolygon(driver, pts, color);
}

void FireEditor::drawPolyline(const std::vector<irr::core::position2di>& pts, irr::video::SColor color, bool dashed, irr::s32 width)
{
    ChartDraw::polyline(driver, pts, color, dashed, width);
}

void FireEditor::drawText(const std::wstring& text, irr::core::position2di at, irr::video::SColor color, bool centred, bool boxed)
{
    irr::core::recti clip = map->getViewport();
    if (boxed) {
        irr::core::dimension2du d = font->getDimension(text.c_str());
        irr::core::recti r(at.X - 2, at.Y, at.X + (irr::s32)d.Width + 2, at.Y + (irr::s32)d.Height);
        if (centred) { r -= irr::core::position2di((irr::s32)d.Width / 2, (irr::s32)d.Height / 2); }
        r.clipAgainst(clip);
        if (r.isValid()) { driver->draw2DRectangle(irr::video::SColor(150, 10, 16, 26), r); }
    }
    ChartDraw::text(font, text, at, color, &clip, centred);
}

void FireEditor::drawMarker(irr::core::position2di at, int shape, irr::video::SColor fill, irr::video::SColor edge, irr::s32 r)
{
    ChartDraw::marker(driver, at, shape, fill, edge, r);
}

void FireEditor::drawShip(const IncidentPoint& p, float heading, irr::video::SColor fill, irr::video::SColor edge, float size)
{
    ChartDraw::ship(driver, map->toScreen(p), heading, fill, edge, size);
}

void FireEditor::drawHighlight(const Pick& p, irr::video::SColor color)
{
    IncidentPoint* pos = positionOf(p);
    if (!pos) { return; }
    ChartDraw::ring(driver, map->toScreen(*pos), 17.0f, color);
}

void FireEditor::drawMap()
{
    map->draw(driver);
    map->drawGraticule(driver, font, rowH + 4, rowH + 2);
}

void FireEditor::drawOverlay()
{
    IncidentConfig& c = scn->incident;
    const irr::core::recti& vp = map->getViewport();

    // Other traffic, faded
    for (size_t i = 0; i < scn->traffic.size(); i++) {
        drawShip(scn->traffic[i].pos, scn->traffic[i].heading, kTrafficFill, kBlack, 11.0f);
        drawText(wide(scn->traffic[i].type), map->toScreen(scn->traffic[i].pos) + irr::core::position2di(12, 4),
            irr::video::SColor(255, 200, 200, 200));
    }

    // SAR boat routes: outbound solid, homeward dashed; thin dotted links to the rafts each boat recovers.
    for (size_t b = 0; b < scn->sarBoats.size() && b < c.sarBoats.size(); b++) {
        irr::video::SColor col = boatColour((int)b);
        const IncidentSarBoat& cfg = c.sarBoats[b];
        std::vector<irr::core::position2di> out;
        out.push_back(map->toScreen(scn->sarBoats[b].pos));
        for (size_t w = 0; w < cfg.outbound.size(); w++) { out.push_back(map->toScreen(cfg.outbound[w])); }
        drawPolyline(out, col, false, 3);
        // From the end of the outbound route she goes for the survivors, then heads home from there.
        irr::core::position2di outEnd = out.back();
        irr::core::position2di scene = scn->hasCasualty ? map->toScreen(scn->casualty.pos) : outEnd;
        std::vector<irr::core::position2di> transit;
        transit.push_back(outEnd);
        transit.push_back(scene);
        drawPolyline(transit, irr::video::SColor(140, col.getRed(), col.getGreen(), col.getBlue()), true, 1);
        for (size_t i = 0; i < c.survivors.size(); i++) {
            if (c.survivors[i].kind == Survivor_MOB || c.survivors[i].boat != (int)b + 1) { continue; }
            std::vector<irr::core::position2di> link;
            link.push_back(outEnd);
            link.push_back(map->toScreen(c.survivors[i].pos));
            drawPolyline(link, irr::video::SColor(160, col.getRed(), col.getGreen(), col.getBlue()), true, 1);
        }
        std::vector<irr::core::position2di> ret;
        ret.push_back(scene);
        if (cfg.inbound.empty()) {
            for (size_t w = cfg.outbound.size(); w > 0; w--) { ret.push_back(map->toScreen(cfg.outbound[w - 1])); }
            ret.push_back(map->toScreen(scn->sarBoats[b].pos));
        }
        else {
            for (size_t w = 0; w < cfg.inbound.size(); w++) { ret.push_back(map->toScreen(cfg.inbound[w])); }
        }
        drawPolyline(ret, col, true, cfg.inbound.empty() ? 1 : 2);
        for (size_t w = 0; w < cfg.outbound.size(); w++) {
            irr::core::position2di s = map->toScreen(cfg.outbound[w]);
            drawMarker(s, Shape_Square, col, kBlack, 4);
            drawText(L"A" + fmtNum((double)w + 1, 0), s + irr::core::position2di(6, -16), col);
        }
        for (size_t w = 0; w < cfg.inbound.size(); w++) {
            irr::core::position2di s = map->toScreen(cfg.inbound[w]);
            bool berth = (w + 1 == cfg.inbound.size());
            drawMarker(s, berth ? Shape_Diamond : Shape_Square, irr::video::SColor(255, 30, 30, 30), col, berth ? 6 : 4);
            drawText(berth ? std::wstring(L"Quai") : L"R" + fmtNum((double)w + 1, 0), s + irr::core::position2di(6, 2), col);
        }
    }

    // Helicopters: base -> scene dashed, scene -> pad dotted.
    for (size_t k = 0; k < c.helos.size(); k++) {
        const IncidentHelo& hl = c.helos[k];
        irr::core::position2di scene = map->toScreen(scn->hasCasualty ? scn->casualty.pos : scn->ownShip.pos);
        std::wstring name = L"H\u00E9lico " + fmtNum((double)k + 1, 0);
        if (hl.hasBase) {
            irr::core::position2di s = map->toScreen(hl.base);
            std::vector<irr::core::position2di> line;
            line.push_back(s); line.push_back(scene);
            drawPolyline(line, kHeloColour, true, 2);
            drawMarker(s, Shape_Circle, irr::video::SColor(255, 40, 60, 90), kHeloColour, 10);
            drawText(L"H", s, kHeloColour, true);
            drawText(name + L" (base)", s + irr::core::position2di(12, -6), kHeloColour);
        }
        if (hl.hasPad) {
            irr::core::position2di s = map->toScreen(hl.pad);
            std::vector<irr::core::position2di> line;
            line.push_back(scene); line.push_back(s);
            drawPolyline(line, irr::video::SColor(150, 255, 255, 120), true, 1);
            drawMarker(s, Shape_Square, irr::video::SColor(255, 40, 60, 90), kHeloColour, 9);
            drawText(L"H", s, kHeloColour, true);
            drawText(name + L" (retour)", s + irr::core::position2di(12, -6), kHeloColour);
        }
    }

    // Survivors, numbered in drop order
    for (size_t i = 0; i < c.survivors.size(); i++) {
        const IncidentSurvivor& sv = c.survivors[i];
        irr::core::position2di s = map->toScreen(sv.pos);
        if (sv.kind == Survivor_MOB) {
            drawMarker(s, Shape_Circle, kMobFill, kBlack, 6);
        }
        else {
            irr::video::SColor col = sv.boat > 0 ? boatColour(sv.boat - 1) : irr::video::SColor(255, 240, 240, 240);
            drawMarker(s, sv.kind == Survivor_Liferaft ? Shape_Square : Shape_Diamond, irr::video::SColor(255, 255, 120, 0), col, 7);
        }
        std::wstring tag = fmtNum((double)i + 1, 0) + (sv.kind == Survivor_MOB ? L"" : (sv.kind == Survivor_Liferaft ? L" LR" : L" RS"));
        drawText(tag, s + irr::core::position2di(8, -8), irr::video::SColor(255, 255, 230, 200));
    }

    // SAR boats at their start
    for (size_t b = 0; b < scn->sarBoats.size(); b++) {
        drawShip(scn->sarBoats[b].pos, scn->sarBoats[b].heading, boatColour((int)b), kBlack, 13.0f);
        drawText(L"SAR " + fmtNum((double)b + 1, 0), map->toScreen(scn->sarBoats[b].pos) + irr::core::position2di(14, -8), boatColour((int)b));
    }

    // Casualty and own ship on top
    if (scn->hasCasualty) {
        irr::core::position2di s = map->toScreen(scn->casualty.pos);
        driver->draw2DPolygon(s, 22.0f, irr::video::SColor(255, 255, 120, 0), 18);
        drawShip(scn->casualty.pos, scn->casualty.heading, kCasualtyFill, irr::video::SColor(255, 255, 220, 0), 16.0f);
        drawText(L"EN FEU", s + irr::core::position2di(20, -10), irr::video::SColor(255, 255, 120, 80));
    }
    drawShip(scn->ownShip.pos, scn->ownShip.heading, kOwnFill, kWhite, 15.0f);
    drawText(L"Navire propre", map->toScreen(scn->ownShip.pos) + irr::core::position2di(18, -8), irr::video::SColor(255, 150, 200, 255));

    if (hover.type != Pick_None && !(hover == selected)) { drawHighlight(hover, kHover); }
    if (selected.type != Pick_None) { drawHighlight(selected, kSelect); }

    // Tool hint across the top of the chart
    std::wstring hint = toolHint();
    irr::core::recti bar(vp.UpperLeftCorner.X, vp.UpperLeftCorner.Y, vp.LowerRightCorner.X, vp.UpperLeftCorner.Y + rowH + 4);
    driver->draw2DRectangle(tool == Tool_Select ? irr::video::SColor(170, 0, 0, 0) : irr::video::SColor(220, 140, 60, 0), bar);
    drawText(hint, irr::core::position2di(bar.UpperLeftCorner.X + 10, bar.UpperLeftCorner.Y + 5), kWhite, false, false);

    // Status line: cursor position and scale bar
    irr::core::recti status(vp.UpperLeftCorner.X, vp.LowerRightCorner.Y - rowH - 2, vp.LowerRightCorner.X, vp.LowerRightCorner.Y);
    driver->draw2DRectangle(irr::video::SColor(170, 0, 0, 0), status);
    std::wstring where = overMap(mouse) ? fmtLatLong(map->toLatLong(mouse)) : std::wstring(L"");
    drawText(where + L"    molette : zoom   clic droit + glisser : d\u00E9placer la carte   Origine : recentrer   fl\u00E8ches gauche/droite : cap du navire s\u00E9lectionn\u00E9",
        irr::core::position2di(status.UpperLeftCorner.X + 10, status.UpperLeftCorner.Y + 4), irr::video::SColor(255, 220, 220, 220), false, false);

    ChartDraw::scaleBar(driver, font, *map, rowH + 2);
}

std::wstring FireEditor::toolHint() const
{
    std::wstring b = boatLabel(selBoat);
    std::wstring h = L"h\u00E9lico " + fmtNum((double)selHelo + 1, 0);
    switch (tool) {
    case Tool_PlaceOwn: return L"Cliquez pour placer le navire propre   (clic droit / \u00C9chap : annuler)";
    case Tool_PlaceCasualty: return L"Cliquez pour placer le navire en feu   (clic droit / \u00C9chap : annuler)";
    case Tool_PlaceBoat: return L"Cliquez pour placer le d\u00E9part de la " + b;
    case Tool_AddOut: return L"Route ALLER de la " + b + L" : cliquez les points dans l'ordre, clic droit pour terminer";
    case Tool_AddRet: return L"Route RETOUR de la " + b + L" : cliquez les points, le dernier = poste \u00E0 quai, clic droit pour terminer";
    case Tool_AddMob: return L"Cliquez pour ajouter des hommes \u00E0 la mer, clic droit pour terminer";
    case Tool_AddLiferaft: return L"Cliquez pour ajouter des liferafts, clic droit pour terminer";
    case Tool_AddRadeau: return L"Cliquez pour ajouter des radeaux de sauvetage, clic droit pour terminer";
    case Tool_PlaceHeloBase: return L"Cliquez pour placer la base de d\u00E9part de l'" + h;
    case Tool_PlaceHeloPad: return L"Cliquez pour placer l'h\u00E9lisurface de retour de l'" + h;
    default: return L"S\u00E9lection : cliquez un \u00E9l\u00E9ment pour le choisir, glissez-le pour le d\u00E9placer, Suppr pour l'effacer";
    }
}

void FireEditor::drawTimeline()
{
    if (tabs->getActiveTab() != TAB_CASUALTY) { return; }
    const IncidentConfig& c = scn->incident;
    irr::core::recti a = timelineArea + tabCasualty->getAbsolutePosition().UpperLeftCorner;
    driver->draw2DRectangle(irr::video::SColor(255, 22, 26, 34), a);
    driver->draw2DRectangleOutline(a, irr::video::SColor(255, 70, 80, 95));

    int n = (int)c.survivors.size();
    float inWater = c.abandonTime + (n > 1 ? (float)(n - 1) * c.survivorInterval : 0.0f);
    float maxT = std::max(c.fireDuration, inWater);
    for (size_t b = 0; b < c.sarBoats.size(); b++) { maxT = std::max(maxT, inWater + c.sarBoats[b].launchDelay); }
    maxT = maxT * 1.04f + 1.0f;

    irr::s32 left = a.UpperLeftCorner.X + 10, right = a.LowerRightCorner.X - 10;
    irr::s32 axis = a.UpperLeftCorner.Y + a.getHeight() / 2;
    irr::f32 scale = (irr::f32)(right - left) / maxT;
    auto xAt = [left, scale](float t) { return left + (irr::s32)(t * scale); };

    // bars: fire building up, survivors going in, foundering
    driver->draw2DRectangle(irr::core::recti(xAt(0), axis - 8, xAt(c.fireSpreadTime), axis - 2),
        irr::video::SColor(255, 255, 200, 0), irr::video::SColor(255, 255, 90, 0), irr::video::SColor(255, 255, 200, 0), irr::video::SColor(255, 255, 90, 0));
    driver->draw2DRectangle(irr::video::SColor(255, 255, 90, 0), irr::core::recti(xAt(c.fireSpreadTime), axis - 8, xAt(c.fireDuration), axis - 2));
    driver->draw2DRectangle(irr::video::SColor(255, 200, 20, 20), irr::core::recti(xAt(c.sinkStartTime()), axis - 8, xAt(c.fireDuration), axis - 2));
    driver->draw2DRectangle(irr::video::SColor(255, 0, 200, 230), irr::core::recti(xAt(c.abandonTime), axis + 2, std::max(xAt(inWater), xAt(c.abandonTime) + 3), axis + 7));
    driver->draw2DLine(irr::core::position2di(left, axis), irr::core::position2di(right, axis), kWhite);

    // minute ticks
    float tick = maxT > 900 ? 120.0f : (maxT > 300 ? 60.0f : 30.0f);
    irr::core::recti clip = a;
    for (float t = 0; t <= maxT; t += tick) {
        irr::s32 x = xAt(t);
        driver->draw2DLine(irr::core::position2di(x, axis - 2), irr::core::position2di(x, axis + 2), kWhite);
    }

    // labelled markers, staggered so they do not overlap
    struct Mark { float t; std::wstring text; irr::video::SColor col; bool below; };
    std::vector<Mark> marks;
    Mark m;
    m.below = false;
    m.t = 0; m.text = L"Ctrl+F"; m.col = kWhite; marks.push_back(m);
    m.t = c.abandonTime; m.text = L"Abandon " + fmtTime(c.abandonTime); m.col = irr::video::SColor(255, 255, 230, 80); marks.push_back(m);
    m.t = c.fireSpreadTime; m.text = L"Feu total " + fmtTime(c.fireSpreadTime); m.col = irr::video::SColor(255, 255, 150, 40); marks.push_back(m);
    m.t = c.sinkStartTime(); m.text = L"Naufrage " + fmtTime(c.sinkStartTime()); m.col = irr::video::SColor(255, 255, 80, 80); marks.push_back(m);
    m.t = c.fireDuration; m.text = L"Coul\u00E9 " + fmtTime(c.fireDuration); m.col = irr::video::SColor(255, 200, 60, 60); marks.push_back(m);
    m.below = true;
    m.t = inWater; m.text = L"\u00C0 l'eau " + fmtTime(inWater); m.col = irr::video::SColor(255, 0, 210, 240); marks.push_back(m);
    for (size_t b = 0; b < c.sarBoats.size(); b++) {
        m.t = inWater + c.sarBoats[b].launchDelay;
        m.text = L"V" + fmtNum((double)b + 1, 0) + L" " + fmtTime(m.t);
        m.col = boatColour((int)b);
        marks.push_back(m);
    }
    irr::s32 lineH = (irr::s32)font->getDimension(L"Ag").Height;
    std::vector<irr::core::recti> placed;
    for (size_t i = 0; i < marks.size(); i++) {
        irr::s32 x = xAt(marks[i].t);
        irr::core::dimension2du d = font->getDimension(marks[i].text.c_str());
        irr::s32 tx = std::min(std::max(x - (irr::s32)d.Width / 2, a.UpperLeftCorner.X + 2), a.LowerRightCorner.X - 2 - (irr::s32)d.Width);
        irr::core::recti r;
        for (int row = 0; row < 3; row++) {
            irr::s32 ty = marks[i].below ? axis + 10 + row * lineH : axis - 12 - (row + 1) * lineH;
            r = irr::core::recti(tx, ty, tx + (irr::s32)d.Width, ty + lineH);
            bool clash = false;
            for (size_t p = 0; p < placed.size(); p++) { if (placed[p].isRectCollided(r)) { clash = true; break; } }
            if (!clash) { break; }
        }
        placed.push_back(r);
        irr::s32 ly = marks[i].below ? r.UpperLeftCorner.Y : r.LowerRightCorner.Y;
        driver->draw2DLine(irr::core::position2di(x, axis), irr::core::position2di(x, ly), marks[i].col);
        font->draw(marks[i].text.c_str(), r, marks[i].col, false, false, &clip);
    }
}

bool FireEditor::run()
{
    while (device->run()) {
        if (backToMenu) { return true; }
        if (needRefresh) { needRefresh = false; refreshAll(); }
        hover = (!dragging && overMap(mouse)) ? pickAt(mouse) : Pick();
        driver->beginScene(true, true, irr::video::SColor(255, 30, 34, 43));
        drawMap();
        drawOverlay();
        driver->draw2DRectangle(irr::video::SColor(255, 30, 34, 43), irr::core::recti(panelX, 0, screenW, screenH));
        guienv->drawAll();
        drawTimeline();
        driver->endScene();
    }
    return false;
}
