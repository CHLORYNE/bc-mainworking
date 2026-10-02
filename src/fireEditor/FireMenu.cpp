/* SCENARIO INCENDIE - fire scenario editor: the start screen. See FireMenu.hpp.
   Source kept ASCII: accented text is written with \u escapes. */
#define _CRT_SECURE_NO_WARNINGS
#include "FireMenu.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

#include "../Utilities.hpp"
#include "../chartView/ChartView.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

std::string FireMenu::rememberedWorld;
std::string FireMenu::rememberedScenario;

namespace {

enum {
    ID_WORLDS = 1, ID_SCENARIOS, ID_NEW, ID_OPEN, ID_DELETE, ID_IMPORT, ID_EXPORT, ID_QUIT,
    ID_CONFIRM_DELETE, ID_IE_COPY, ID_IE_PASTE, ID_IE_OK, ID_IE_CANCEL
};

const irr::video::SColor kHeading(255, 255, 160, 80);
const irr::video::SColor kText(255, 225, 232, 240);
const irr::video::SColor kDim(255, 150, 165, 180);
const irr::video::SColor kFire(255, 255, 140, 100);

// Names (folders, models) are 8-bit, widened as the standard editor does.
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

// description.txt files are UTF-8; fall back to Latin-1 for bytes that are not valid UTF-8.
std::wstring fromUtf8(const std::string& s)
{
    std::wstring out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { out += (wchar_t)c; i++; continue; }
        int extra = (c >> 5) == 0x6 ? 1 : (c >> 4) == 0xE ? 2 : (c >> 3) == 0x1E ? 3 : -1;
        bool ok = extra > 0 && i + extra < s.size() + 1;
        unsigned long cp = (extra == 1) ? (c & 0x1F) : (extra == 2) ? (c & 0x0F) : (c & 0x07);
        for (int k = 1; ok && k <= extra; k++) {
            if (i + k >= s.size() || ((unsigned char)s[i + k] >> 6) != 0x2) { ok = false; break; }
            cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        }
        if (!ok) { out += (wchar_t)c; i++; continue; }
        if (cp < 0x10000) { out += (wchar_t)cp; }
        i += extra + 1;
    }
    return out;
}

std::wstring fmt(const wchar_t* format, double v)
{
    wchar_t buf[64];
    swprintf(buf, 64, format, v);
    return buf;
}

std::wstring fmtTime(float secs)
{
    int t = (int)std::floor(secs + 0.5f);
    if (t < 0) { t = 0; }
    wchar_t buf[16];
    swprintf(buf, 16, L"%02d:%02d", t / 60, t % 60);
    return buf;
}

std::wstring fmtLat(double lat)
{
    double v = std::fabs(lat);
    wchar_t buf[32];
    swprintf(buf, 32, L"%02d\u00B0%04.1f'%lc", (int)v, (v - (int)v) * 60.0, lat >= 0 ? L'N' : L'S');
    return buf;
}

std::wstring fmtLon(double lon)
{
    double v = std::fabs(lon);
    wchar_t buf[32];
    swprintf(buf, 32, L"%03d\u00B0%04.1f'%lc", (int)v, (v - (int)v) * 60.0, lon >= 0 ? L'E' : L'W');
    return buf;
}

bool removeDirectory(const std::string& path)
{
#ifdef _WIN32
    std::string from = path;
    std::replace(from.begin(), from.end(), '/', '\\');
    from.append(1, '\0');   // SHFileOperation wants a double-null-terminated list
    SHFILEOPSTRUCTA op;
    memset(&op, 0, sizeof(op));
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    return SHFileOperationA(&op) == 0;
#else
    DIR* dir = opendir(path.c_str());
    if (!dir) { return false; }
    struct dirent* e;
    bool ok = true;
    while ((e = readdir(dir)) != 0) {
        std::string name = e->d_name;
        if (name == "." || name == "..") { continue; }
        std::string child = path + "/" + name;
        struct stat st;
        if (stat(child.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) { ok = removeDirectory(child) && ok; }
        else { ok = (unlink(child.c_str()) == 0) && ok; }
    }
    closedir(dir);
    return (rmdir(path.c_str()) == 0) && ok;
#endif
}

} // namespace

void listSubdirectories(irr::IrrlichtDevice* device, std::vector<std::string>& out, const std::string& path)
{
    irr::io::IFileSystem* fs = device->getFileSystem();
    irr::io::path cwd = fs->getWorkingDirectory();
    if (!fs->changeWorkingDirectoryTo(path.c_str())) { return; }
    irr::io::IFileList* list = fs->createFileList();
    if (list) {
        for (irr::u32 i = 0; i < list->getFileCount(); i++) {
            if (list->isDirectory(i) && list->getFileName(i).findFirst('.') != 0) {
                std::string name = list->getFileName(i).c_str();
                if (std::find(out.begin(), out.end(), name) == out.end()) { out.push_back(name); }
            }
        }
        list->drop();
    }
    fs->changeWorkingDirectoryTo(cwd);
}

FireMenu::FireMenu(irr::IrrlichtDevice* dev, const std::string& path, const std::wstring& message)
    : device(dev), scenariosPath(path), choice(-1), worldList(0), scenarioList(0), mapDetails(0), scenarioDetails(0),
    statusText(0), ieWindow(0), ieText(0), ieImporting(false)
{
    env = device->getGUIEnvironment();
    userFolder = Utilities::getUserDir();
    rowH = std::max(24, (irr::s32)env->getSkin()->getFont()->getDimension(L"Ag").Height + 10);
    build(message);
}

FireMenu::~FireMenu()
{
    device->setEventReceiver(0);
    env->clear();
}

void FireMenu::build(const std::wstring& message)
{
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::s32 w = (irr::s32)driver->getScreenSize().Width, h = (irr::s32)driver->getScreenSize().Height;

    irr::gui::IGUIStaticText* title = env->addStaticText(
        L"\u00C9diteur de sc\u00E9narios incendie / SAR\nNAUTITECH S.A.R.L",
        irr::core::recti(w / 10, h / 40, w * 9 / 10, h / 40 + rowH * 2), false, true);
    title->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
    title->setOverrideColor(kHeading);

    irr::s32 top = h / 40 + rowH * 2 + 14;
    irr::s32 x1 = w / 20;
    irr::s32 colW = (w - 2 * x1 - 30) / 2;
    irr::s32 x2 = x1 + colW + 30;
    irr::s32 bottom = h - rowH * 2 - 24;                 // above the status line and Quit
    irr::s32 buttonsTop = bottom - (2 * rowH + 6);
    irr::s32 listBottom = top + rowH + (buttonsTop - top - rowH) * 45 / 100;

    // Charts
    env->addStaticText(L"Cartes - nouvel exercice incendie :", irr::core::recti(x1, top, x1 + colW, top + rowH));
    worldList = env->addListBox(irr::core::recti(x1, top + rowH, x1 + colW, listBottom), 0, ID_WORLDS, true);
    mapDetails = env->addListBox(irr::core::recti(x1, listBottom + 8, x1 + colW, buttonsTop - 8), 0, -1, true);
    env->addButton(irr::core::recti(x1, buttonsTop, x1 + colW, buttonsTop + rowH), 0, ID_NEW, L"Cr\u00E9er un nouvel exercice sur cette carte");

    // Exercises
    env->addStaticText(L"Exercices existants :", irr::core::recti(x2, top, x2 + colW, top + rowH));
    scenarioList = env->addListBox(irr::core::recti(x2, top + rowH, x2 + colW, listBottom), 0, ID_SCENARIOS, true);
    scenarioDetails = env->addListBox(irr::core::recti(x2, listBottom + 8, x2 + colW, buttonsTop - 8), 0, -1, true);
    irr::s32 half = (colW - 8) / 2;
    env->addButton(irr::core::recti(x2, buttonsTop, x2 + half, buttonsTop + rowH), 0, ID_OPEN, L"Ouvrir l'exercice");
    irr::gui::IGUIButton* del = env->addButton(irr::core::recti(x2 + half + 8, buttonsTop, x2 + colW, buttonsTop + rowH), 0, ID_DELETE, L"Supprimer l'exercice");
    del->setOverrideColor(irr::video::SColor(255, 255, 110, 110));
    irr::s32 row2 = buttonsTop + rowH + 6;
    env->addButton(irr::core::recti(x2, row2, x2 + half, row2 + rowH), 0, ID_IMPORT, L"Importer");
    env->addButton(irr::core::recti(x2 + half + 8, row2, x2 + colW, row2 + rowH), 0, ID_EXPORT, L"Exporter");

    statusText = env->addStaticText(message.c_str(), irr::core::recti(x1, bottom + 10, x2 + colW - 170, h - 8), false, true);
    statusText->setOverrideColor(kDim);
    env->addButton(irr::core::recti(x2 + colW - 160, h - rowH - 16, x2 + colW, h - 16), 0, ID_QUIT, L"Quitter");

    refreshLists();
}

void FireMenu::refreshLists()
{
    worlds.clear();
    scenarios.clear();
    hasIncident.clear();
    listSubdirectories(device, worlds, "World/");
    listSubdirectories(device, worlds, userFolder + "World/");
    listSubdirectories(device, scenarios, scenariosPath);
    std::sort(worlds.begin(), worlds.end());
    std::sort(scenarios.begin(), scenarios.end());
    for (size_t i = 0; i < scenarios.size(); i++) {
        hasIncident.push_back(Utilities::pathExists(scenariosPath + scenarios[i] + "/incident.ini"));
    }

    worldList->clear();
    for (size_t i = 0; i < worlds.size(); i++) { worldList->addItem(wide(worlds[i]).c_str()); }
    scenarioList->clear();
    for (size_t i = 0; i < scenarios.size(); i++) {
        std::wstring item = wide(scenarios[i]);
        if (hasIncident[i]) { item += L"   [incendie]"; }
        scenarioList->addItem(item.c_str());
    }

    std::vector<std::string>::iterator wi = std::find(worlds.begin(), worlds.end(), rememberedWorld);
    worldList->setSelected(wi != worlds.end() ? (irr::s32)(wi - worlds.begin()) : (worlds.empty() ? -1 : 0));
    std::vector<std::string>::iterator si = std::find(scenarios.begin(), scenarios.end(), rememberedScenario);
    scenarioList->setSelected(si != scenarios.end() ? (irr::s32)(si - scenarios.begin()) : (scenarios.empty() ? -1 : 0));
    updateMapDetails();
    updateScenarioDetails();
}

void FireMenu::addWrapped(irr::gui::IGUIListBox* box, const std::wstring& text)
{
    addWrapped(box, text, kText);
}

// Adds text to a details list, wrapped on words to the list's width.
void FireMenu::addWrapped(irr::gui::IGUIListBox* box, const std::wstring& text, irr::video::SColor colour)
{
    irr::gui::IGUIFont* font = env->getSkin()->getFont();
    irr::s32 maxW = box->getAbsolutePosition().getWidth() - 30;
    std::wstring line, word;
    std::vector<std::wstring> lines;
    for (size_t i = 0; i <= text.size(); i++) {
        wchar_t c = (i < text.size()) ? text[i] : L' ';
        if (c != L' ') { word += c; continue; }
        std::wstring candidate = line.empty() ? word : line + L" " + word;
        if (!line.empty() && (irr::s32)font->getDimension(candidate.c_str()).Width > maxW) {
            lines.push_back(line);
            line = L"   " + word;
        }
        else { line = candidate; }
        word.clear();
    }
    lines.push_back(line);
    for (size_t i = 0; i < lines.size(); i++) {
        irr::u32 idx = box->addItem(lines[i].c_str());
        box->setItemOverrideColor(idx, irr::gui::EGUI_LBC_TEXT, colour);
        box->setItemOverrideColor(idx, irr::gui::EGUI_LBC_TEXT_HIGHLIGHT, colour);
    }
}

std::string FireMenu::chosenWorld() const
{
    irr::s32 i = worldList ? worldList->getSelected() : -1;
    return (i >= 0 && i < (irr::s32)worlds.size()) ? worlds[i] : rememberedWorld;
}

std::string FireMenu::chosenScenario() const
{
    irr::s32 i = scenarioList ? scenarioList->getSelected() : -1;
    return (i >= 0 && i < (irr::s32)scenarios.size()) ? scenarios[i] : rememberedScenario;
}

void FireMenu::updateMapDetails()
{
    mapDetails->clear();
    irr::s32 sel = worldList->getSelected();
    if (sel < 0 || sel >= (irr::s32)worlds.size()) { addWrapped(mapDetails, L"S\u00E9lectionnez une carte.", kDim); return; }
    const std::string& world = worlds[sel];
    rememberedWorld = world;
    addWrapped(mapDetails, L"Carte : " + wide(world), kHeading);

    // Description, user folder first (UTF-8), as the standard editor shows it.
    std::string descPath = "World/" + world + "/description.txt";
    if (Utilities::pathExists(userFolder + descPath)) { descPath = userFolder + descPath; }
    std::ifstream desc(descPath.c_str());
    bool any = false;
    std::string line;
    while (desc.is_open() && std::getline(desc, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        if (line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) { line = line.substr(3); }
        addWrapped(mapDetails, fromUtf8(line));
        any = true;
    }
    if (!any) { addWrapped(mapDetails, L"Aucune description (description.txt) pour cette carte.", kDim); }

    double s, w, latE, lonE;
    if (ChartView::worldBounds(world, s, w, latE, lonE)) {
        double kmX = lonE * 111.32 * std::cos((s + latE / 2.0) * 3.14159265358979 / 180.0);
        double kmZ = latE * 111.32;
        mapDetails->addItem(L"");
        addWrapped(mapDetails, L"\u00C9tendue : " + fmt(L"%.1f", kmX) + L" km x " + fmt(L"%.1f", kmZ) + L" km", kDim);
        addWrapped(mapDetails, L"Latitude : " + fmtLat(s) + L" \u00E0 " + fmtLat(s + latE), kDim);
        addWrapped(mapDetails, L"Longitude : " + fmtLon(w) + L" \u00E0 " + fmtLon(w + lonE), kDim);
    }
    int count = 0, fires = 0;
    for (size_t i = 0; i < scenarios.size(); i++) {
        IncidentIni::Map e;
        if (IncidentIni::read(scenariosPath + scenarios[i] + "/environment.ini", e) && IncidentIni::str(e, "Setting") == world) {
            count++;
            if (hasIncident[i]) { fires++; }
        }
    }
    addWrapped(mapDetails, L"Exercices sur cette carte : " + fmt(L"%.0f", count) + L" (dont " + fmt(L"%.0f", fires) + L" incendie)", kDim);
}

void FireMenu::updateScenarioDetails()
{
    scenarioDetails->clear();
    irr::s32 sel = scenarioList->getSelected();
    if (sel < 0 || sel >= (irr::s32)scenarios.size()) { addWrapped(scenarioDetails, L"S\u00E9lectionnez un exercice pour voir ses d\u00E9tails.", kDim); return; }
    rememberedScenario = scenarios[sel];

    FireScenario s;
    std::string error;
    if (!s.load(scenariosPath + scenarios[sel], scenarios[sel], error)) {
        addWrapped(scenarioDetails, wide(error), kFire);
        return;
    }
    addWrapped(scenarioDetails, L"Exercice : " + wide(s.name), kHeading);
    int minutes = (int)std::floor(s.startTimeHours * 60.0f + 0.5f);
    wchar_t buf[128];
    swprintf(buf, 128, L"Carte : %ls  |  Heure : %02d:%02d  |  Date : %02u/%02u/%04u",
        wide(s.worldName).c_str(), (minutes / 60) % 24, minutes % 60, s.day, s.month, s.year);
    addWrapped(scenarioDetails, buf);
    addWrapped(scenarioDetails, L"M\u00E9t\u00E9o : " + fmt(L"%.1f", s.weather) + L"  |  Visibilit\u00E9 : " + fmt(L"%.1f", s.visibility) +
        L" NM  |  Pluie : " + fmt(L"%.1f", s.rain));
    addWrapped(scenarioDetails, L"Vent : " + fmt(L"%.1f", s.windSpeed) + L" nds, direction " + fmt(L"%.0f", s.windDirection) + L"\u00B0");
    scenarioDetails->addItem(L"");
    addWrapped(scenarioDetails, L"Navire propre : " + wide(s.ownShip.type) + L" (cap " + fmt(L"%.0f", s.ownShip.heading) +
        L"\u00B0, " + fmt(L"%.1f", s.ownShip.speed) + L" nds)");

    if (s.hadIncidentFile) {
        if (s.hasCasualty) { addWrapped(scenarioDetails, L"Navire en feu : " + wide(s.casualty.type), kFire); }
        for (size_t b = 0; b < s.sarBoats.size(); b++) {
            addWrapped(scenarioDetails, L"Vedette SAR " + fmt(L"%.0f", (double)b + 1) + L" : " + wide(s.sarBoats[b].type));
        }
        addWrapped(scenarioDetails, L"Autres navires : " + fmt(L"%.0f", (double)s.traffic.size()));
        scenarioDetails->addItem(L"");
        const IncidentConfig& c = s.incident;
        addWrapped(scenarioDetails, L"--- Incendie / SAR ---", kFire);
        addWrapped(scenarioDetails, L"Abandon T+" + fmtTime(c.abandonTime) + L"  |  Naufrage T+" + fmtTime(c.sinkStartTime()) +
            L"  |  Coul\u00E9 T+" + fmtTime(c.fireDuration));
        int mobs = 0, rafts = 0;
        for (size_t i = 0; i < c.survivors.size(); i++) { if (c.survivors[i].kind == Survivor_MOB) { mobs++; } else { rafts++; } }
        addWrapped(scenarioDetails, L"Naufrag\u00E9s : " + fmt(L"%.0f", mobs) + L" homme(s) \u00E0 la mer, " + fmt(L"%.0f", rafts) + L" radeau(x)");
        addWrapped(scenarioDetails, L"H\u00E9licopt\u00E8res : " + fmt(L"%.0f", (double)c.helos.size()) + L", sur zone " + fmtTime(c.heloDelay) +
            L" apr\u00E8s l'appel OSC");
        addWrapped(scenarioDetails, L"Centre SAR : " + wide(c.coordinationCentre));
    }
    else {
        int others = (int)s.traffic.size() + (int)s.sarBoats.size() + (s.hasCasualty ? 1 : 0);
        addWrapped(scenarioDetails, L"Autres navires : " + fmt(L"%.0f", others));
        scenarioDetails->addItem(L"");
        addWrapped(scenarioDetails, L"Pas encore de r\u00E9glages incendie : le simulateur utilise ses valeurs par d\u00E9faut.", kDim);
    }
    if (!s.description.empty()) {
        scenarioDetails->addItem(L"");
        std::string d = s.description;
        size_t start = 0;
        while (start <= d.size()) {
            size_t nl = d.find('\n', start);
            std::string part = d.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
            part.erase(std::remove(part.begin(), part.end(), '\r'), part.end());
            addWrapped(scenarioDetails, wide(part), kDim);
            if (nl == std::string::npos) { break; }
            start = nl + 1;
        }
    }
}

void FireMenu::setStatus(const std::wstring& text, bool error)
{
    statusText->setText(text.c_str());
    statusText->setOverrideColor(error ? irr::video::SColor(255, 255, 120, 120) : irr::video::SColor(255, 130, 220, 130));
}

void FireMenu::deleteSelectedScenario()
{
    irr::s32 sel = scenarioList->getSelected();
    if (sel < 0 || sel >= (irr::s32)scenarios.size()) { return; }
    std::string name = scenarios[sel];
    if (removeDirectory(scenariosPath + name)) {
        setStatus(L"Exercice supprim\u00E9 : " + wide(name), false);
        rememberedScenario.clear();
    }
    else {
        setStatus(L"Impossible de supprimer compl\u00E8tement " + wide(scenariosPath + name), true);
    }
    refreshLists();
}

void FireMenu::openImportExport(bool importing)
{
    std::string exported;
    if (!importing) {
        irr::s32 sel = scenarioList->getSelected();
        if (sel < 0 || sel >= (irr::s32)scenarios.size()) { setStatus(L"S\u00E9lectionnez d'abord l'exercice \u00E0 exporter.", true); return; }
        std::string error;
        if (!FireScenario::exportText(scenariosPath + scenarios[sel], scenarios[sel], exported, error)) {
            setStatus(wide(error), true);
            return;
        }
    }
    ieImporting = importing;

    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::s32 w = (irr::s32)driver->getScreenSize().Width, h = (irr::s32)driver->getScreenSize().Height;
    ieWindow = env->addWindow(irr::core::recti(w / 20, h / 20, w - w / 20, h - h / 20), true,
        importing ? L"Importer un exercice" : L"Exporter un exercice");
    ieWindow->getCloseButton()->setVisible(false);
    irr::s32 ww = ieWindow->getRelativePosition().getWidth(), wh = ieWindow->getRelativePosition().getHeight();
    const wchar_t* help = importing
        ? L"Collez ci-dessous le texte d'un exercice export\u00E9 (par cet \u00E9diteur ou par l'\u00E9diteur de sc\u00E9narios standard), puis cliquez sur \"Importer et ouvrir\". L'exercice s'ouvre dans l'\u00E9diteur : enregistrez-le pour le garder."
        : L"Copiez ce texte pour transmettre l'exercice. La premi\u00E8re ligne est le format de l'\u00E9diteur standard ; la ligne @INCIDENT@ contient les r\u00E9glages incendie / SAR.";
    env->addStaticText(help, irr::core::recti(20, rowH + 10, ww - 20, rowH * 3 + 10), false, true, ieWindow);
    ieText = env->addEditBox(wide(exported).c_str(), irr::core::recti(20, rowH * 3 + 16, ww - 20, wh - rowH * 2 - 10), true, ieWindow);
    ieText->setMultiLine(true);
    ieText->setWordWrap(true);
    ieText->setAutoScroll(true);
    ieText->setMax(0);
    ieText->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_UPPERLEFT);
    irr::s32 by = wh - rowH - 10, bw = (ww - 40 - 20) / 3;
    if (importing) {
        env->addButton(irr::core::recti(20, by, 20 + bw, by + rowH - 2), ieWindow, ID_IE_PASTE, L"Coller depuis le presse-papiers");
        env->addButton(irr::core::recti(30 + bw, by, 30 + 2 * bw, by + rowH - 2), ieWindow, ID_IE_OK, L"Importer et ouvrir");
    }
    else {
        env->addButton(irr::core::recti(20, by, 20 + bw, by + rowH - 2), ieWindow, ID_IE_COPY, L"Copier dans le presse-papiers");
    }
    env->addButton(irr::core::recti(40 + 2 * bw, by, 40 + 3 * bw, by + rowH - 2), ieWindow, ID_IE_CANCEL, importing ? L"Annuler" : L"Fermer");
    env->setFocus(ieText);
}

void FireMenu::closeImportExport()
{
    if (ieWindow) { ieWindow->remove(); }
    ieWindow = 0;
    ieText = 0;
}

void FireMenu::importFromText()
{
    std::string text = narrow(ieText->getText());
    std::string error;
    if (!imported.importText(text, error)) {
        setStatus(wide(error), true);
        return;
    }
    if (std::find(worlds.begin(), worlds.end(), imported.worldName) == worlds.end()) {
        setStatus(L"La carte \"" + wide(imported.worldName) + L"\" de cet exercice n'est pas install\u00E9e sur cet ordinateur.", true);
        return;
    }
    closeImportExport();
    choice = Choice_Import;
}

bool FireMenu::OnEvent(const irr::SEvent& event)
{
    if (event.EventType != irr::EET_GUI_EVENT || !event.GUIEvent.Caller) { return false; }
    irr::s32 id = event.GUIEvent.Caller->getID();
    switch (event.GUIEvent.EventType) {
    case irr::gui::EGET_LISTBOX_CHANGED:
        if (id == ID_WORLDS) { updateMapDetails(); }
        if (id == ID_SCENARIOS) { updateScenarioDetails(); }
        return false;
    case irr::gui::EGET_LISTBOX_SELECTED_AGAIN:   // double click
        if (id == ID_WORLDS && worldList->getSelected() >= 0) { choice = Choice_New; }
        if (id == ID_SCENARIOS && scenarioList->getSelected() >= 0) { choice = Choice_Open; }
        return false;
    case irr::gui::EGET_MESSAGEBOX_YES:
        if (id == ID_CONFIRM_DELETE) { deleteSelectedScenario(); }
        return false;
    case irr::gui::EGET_BUTTON_CLICKED:
        switch (id) {
        case ID_NEW: if (worldList->getSelected() >= 0) { choice = Choice_New; } break;
        case ID_OPEN: if (scenarioList->getSelected() >= 0) { choice = Choice_Open; } break;
        case ID_QUIT: choice = Choice_Quit; break;
        case ID_DELETE: {
            irr::s32 sel = scenarioList->getSelected();
            if (sel < 0 || sel >= (irr::s32)scenarios.size()) { break; }
            std::wstring text = L"Supprimer d\u00E9finitivement l'exercice \"" + wide(scenarios[sel]) + L"\" ?";
            env->addMessageBox(L"Supprimer l'exercice", text.c_str(), true, irr::gui::EMBF_YES | irr::gui::EMBF_NO, 0, ID_CONFIRM_DELETE);
            break;
        }
        case ID_IMPORT: openImportExport(true); break;
        case ID_EXPORT: openImportExport(false); break;
        case ID_IE_COPY:
            if (ieText) {
                device->getOSOperator()->copyToClipboard(narrow(ieText->getText()).c_str());
                setStatus(L"Texte de l'exercice copi\u00E9 dans le presse-papiers.", false);
            }
            break;
        case ID_IE_PASTE:
            if (ieText) {
                const irr::c8* clip = device->getOSOperator()->getTextFromClipboard();
                if (clip) { ieText->setText(wide(clip).c_str()); }
            }
            break;
        case ID_IE_OK: if (ieText) { importFromText(); } break;
        case ID_IE_CANCEL: closeImportExport(); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

FireMenu::Choice FireMenu::run()
{
    device->setEventReceiver(this);
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    while (device->run() && choice < 0) {
        driver->beginScene(true, true, irr::video::SColor(255, 30, 34, 43));
        env->drawAll();
        driver->endScene();
    }
    if (choice < 0) { choice = Choice_Quit; }   // window closed
    if (choice == Choice_New) { rememberedWorld = chosenWorld(); }
    if (choice == Choice_Open) { rememberedScenario = chosenScenario(); }
    return (Choice)choice;
}
