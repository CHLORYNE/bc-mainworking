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
#include "../Constants.hpp"
#include "../UiTheme.hpp"
#include "../Utilities.hpp"
#include "../ScenarioDataStructure.hpp"

#include <iostream>
#include <cstdio>

//using namespace irr;

namespace {

    //Station names as typed ("PC-1, PC-2"): trimmed, empty entries dropped.
    std::vector<std::wstring> splitHosts(const std::wstring& text)
    {
        std::vector<std::wstring> hosts;
        std::wstring item;
        for (size_t i = 0; i <= text.size(); i++) {
            if (i == text.size() || text[i] == L',') {
                const std::wstring t = Utilities::trim(item);
                if (!t.empty()) { hosts.push_back(t); }
                item.clear();
            }
            else {
                item += text[i];
            }
        }
        return hosts;
    }

    std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

    std::wstring timeText(irr::f32 hours)
    {
        const int minutes = (int)(hours * 60.0f + 0.5f);
        wchar_t buf[16];
        swprintf(buf, 16, L"%02d:%02d", (minutes / 60) % 24, minutes % 60);
        return buf;
    }

    struct SetupTexts {
        std::wstring title, subtitle, step1, step2, emptyTitle, emptyHelp, world, ships, start, hostLabel, hostHint,
            station, ship, noHosts, tooMany, tooManyEnd, before, launch, connecting;
    };

    SetupTexts setupTexts(bool french)
    {
        SetupTexts t;
        if (french) {
            t.title = L"Hub multijoueur";
            t.subtitle = L"Relier plusieurs postes du simulateur dans un m\u00EAme exercice";
            t.step1 = L"Exercice multijoueur";
            t.step2 = L"Postes participants";
            t.emptyTitle = L"Aucun exercice multijoueur";
            t.emptyHelp = L"Les exercices multijoueurs sont les dossiers de Scenarios dont le nom se termine par _mp. Cr\u00E9ez-en un avec l'\u00E9diteur d'exercices, en choisissant le mode multijoueur.";
            t.world = L"Zone";
            t.ships = L"Navires (un par poste)";
            t.start = L"D\u00E9but";
            t.hostLabel = L"Nom r\u00E9seau des PC, s\u00E9par\u00E9s par des virgules";
            t.hostHint = L"Exemple : PASSERELLE-1, PASSERELLE-2";
            t.station = L"Poste";
            t.ship = L"Navire";
            t.noHosts = L"Indiquez au moins un poste.";
            t.tooMany = L"Trop de postes : cet exercice n'a que ";
            t.tooManyEnd = L" navire(s), un par poste.";
            t.before = L"Avant de lancer : d\u00E9marrez le simulateur sur chaque poste et cochez \u00AB Mode multijoueur \u00BB.";
            t.launch = L"Lancer l'exercice";
            t.connecting = L"Connexion aux postes...";
        }
        else {
            t.title = L"Multiplayer hub";
            t.subtitle = L"Link several simulator stations in one exercise";
            t.step1 = L"Multiplayer exercise";
            t.step2 = L"Stations taking part";
            t.emptyTitle = L"No multiplayer exercise";
            t.emptyHelp = L"Multiplayer exercises are the folders in Scenarios whose name ends with _mp. Create one with the scenario editor, choosing multiplayer mode.";
            t.world = L"Area";
            t.ships = L"Ships (one per station)";
            t.start = L"Start";
            t.hostLabel = L"Network names of the PCs, separated by commas";
            t.hostHint = L"Example: BRIDGE-1, BRIDGE-2";
            t.station = L"Station";
            t.ship = L"Ship";
            t.noHosts = L"Enter at least one station.";
            t.tooMany = L"Too many stations: this exercise has only ";
            t.tooManyEnd = L" ship(s), one per station.";
            t.before = L"Before launching: start the simulator on each station and tick \u00AB Multiplayer mode \u00BB.";
            t.launch = L"Launch the exercise";
            t.connecting = L"Connecting to the stations...";
        }
        return t;
    }

    //Background, header and the two cards of the set-up screen. The scenario list, the station box
    //and the start button are GUI elements placed on it (see the *Rect() functions).
    class SetupPanel : public irr::gui::IGUIElement
    {
    public:
        SetupPanel(irr::gui::IGUIEnvironment* env, const SetupTexts& texts, irr::gui::IGUIFont* titleFont,
            irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* smallFont)
            : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, env, env->getRootGUIElement(), -1,
                irr::core::rect<irr::s32>(irr::core::position2di(0, 0), env->getVideoDriver()->getScreenSize())),
            texts(texts), titleFont(titleFont), textFont(textFont), smallFont(smallFont), hasScenario(false), emptyList(false),
            attention(false), connecting(false)
        {
            const irr::f32 W = (irr::f32)AbsoluteRect.getWidth(), H = (irr::f32)AbsoluteRect.getHeight();
            const irr::f32 top = 112, bottom = H - 104;
            const irr::f32 split = irr::core::max_(380.0f, W * 0.44f);
            leftCard = irr::core::rect<irr::f32>(28, top, split - 10, bottom);
            rightCard = irr::core::rect<irr::f32>(split + 10, top, W - 28, bottom);
            const irr::f32 rowH = Ui::textHeight(textFont) + 20;
            listArea = irr::core::rect<irr::f32>(leftCard.UpperLeftCorner.X + 20, leftCard.UpperLeftCorner.Y + 62,
                leftCard.LowerRightCorner.X - 20, leftCard.UpperLeftCorner.Y + 62 + irr::core::max_(rowH * 4, leftCard.getHeight() * 0.48f));
            hostArea = irr::core::rect<irr::f32>(rightCard.UpperLeftCorner.X + 20, rightCard.UpperLeftCorner.Y + 92,
                rightCard.LowerRightCorner.X - 20, rightCard.UpperLeftCorner.Y + 92 + rowH);
            startArea = irr::core::rect<irr::f32>(W - 28 - 280, H - 80, W - 28, H - 30);
        }

        irr::core::rect<irr::s32> listRect() const { return Ui::toI(listArea); }
        irr::core::rect<irr::s32> hostRect() const
        {
            irr::core::rect<irr::f32> r = hostArea;
            r.UpperLeftCorner.X += 14; r.LowerRightCorner.X -= 14;
            return Ui::toI(r);
        }
        irr::core::rect<irr::s32> startRect() const { return Ui::toI(startArea); }

        void setEmptyList(bool empty) { emptyList = empty; }
        void setScenario(const ScenarioData* data)
        {
            hasScenario = (data != 0);
            if (data) { scenario = *data; }
        }
        void setHosts(const std::vector<std::wstring>& h) { hosts = h; }
        void setHostBox(irr::gui::IGUIEditBox* box) { hostBox = box; }
        void setAttention(bool on) { attention = on; } //problem shown in red after a refused start
        void setConnecting(bool on) { connecting = on; }

        //Can the exercise start with this scenario and these stations?
        bool ready() const
        {
            return hasScenario && !hosts.empty() && hosts.size() <= scenario.otherShipsData.size();
        }

        virtual void draw()
        {
            if (!IsVisible) { return; }
            irr::video::IVideoDriver* driver = Environment->getVideoDriver();
            const irr::f32 W = (irr::f32)AbsoluteRect.getWidth(), H = (irr::f32)AbsoluteRect.getHeight();
            driver->draw2DRectangle(irr::core::rect<irr::s32>(0, 0, (irr::s32)W, (irr::s32)H), irr::video::SColor(255, 16, 32, 58),
                irr::video::SColor(255, 16, 32, 58), Ui::backgroundDeep, Ui::backgroundDeep);

            irr::gui::PanelBatch b;
            b.begin(driver);
            //Header: hub badge, then a rule under it.
            const irr::core::vector2df badge(56, 56);
            b.disc(badge, 26, irr::video::SColor(70, 64, 156, 240), irr::video::SColor(70, 64, 156, 240));
            Ui::networkIcon(b, badge, 14, Ui::accentHi);
            b.rect(irr::core::rect<irr::f32>(28, 96, W - 28, 97), Ui::rule);
            Ui::card(b, leftCard, 14);
            Ui::card(b, rightCard, 14);
            stepBadge(b, leftCard);
            stepBadge(b, rightCard);
            //Fields behind the list and the station box.
            Ui::roundRect(b, listArea, 10, Ui::field, Ui::field);
            Ui::roundRectOutline(b, listArea, 10, 1.0f, Ui::edge);
            const bool hostFocus = hostBox && Environment->hasFocus(hostBox);
            const bool hostProblem = attention && (hosts.empty() || (hasScenario && hosts.size() > scenario.otherShipsData.size()));
            Ui::roundRect(b, hostArea, 10, Ui::field, Ui::field);
            Ui::roundRectOutline(b, hostArea, 10, 1.0f, hostProblem ? Ui::danger : (hostFocus ? Ui::accentHi : Ui::edge));
            //Assignment rows: station chip -> ship.
            const irr::f32 rowH = Ui::textHeight(textFont) + 16;
            irr::f32 y = assignmentTop();
            const size_t shown = assignmentRows();
            for (size_t i = 0; i < shown; i++) {
                const bool over = hasScenario && i >= scenario.otherShipsData.size();
                const irr::core::rect<irr::f32> row(rightCard.UpperLeftCorner.X + 20, y, rightCard.LowerRightCorner.X - 20, y + rowH - 4);
                Ui::roundRect(b, row, 8, over ? irr::video::SColor(255, 58, 24, 32) : irr::video::SColor(255, 16, 30, 52),
                    over ? irr::video::SColor(255, 48, 20, 28) : irr::video::SColor(255, 13, 25, 44));
                const irr::core::vector2df a(row.getCenter().X - 6, row.getCenter().Y);
                b.line(irr::core::vector2df(a.X - 10, a.Y), irr::core::vector2df(a.X + 8, a.Y), 1.6f, Ui::textFaint);
                b.line(irr::core::vector2df(a.X + 3, a.Y - 5), irr::core::vector2df(a.X + 8, a.Y), 1.6f, Ui::textFaint);
                b.line(irr::core::vector2df(a.X + 3, a.Y + 5), irr::core::vector2df(a.X + 8, a.Y), 1.6f, Ui::textFaint);
                y += rowH;
            }
            b.flush();

            //Text
            Ui::drawText(titleFont, texts.title, irr::core::rect<irr::f32>(96, 26, W - 28, 56), Ui::text);
            Ui::drawText(smallFont, texts.subtitle, irr::core::rect<irr::f32>(96, 58, W - 28, 80), Ui::textDim);
            stepTitle(leftCard, 1, texts.step1);
            stepTitle(rightCard, 2, texts.step2);

            //Left card: empty-list help, or the chosen exercise's details.
            const irr::f32 lx0 = leftCard.UpperLeftCorner.X + 24, lx1 = leftCard.LowerRightCorner.X - 24;
            if (emptyList) {
                const irr::f32 ex0 = listArea.UpperLeftCorner.X + 20, ex1 = listArea.LowerRightCorner.X - 20;
                const irr::f32 cy = listArea.UpperLeftCorner.Y + 26;
                Ui::drawText(textFont, texts.emptyTitle, irr::core::rect<irr::f32>(ex0, cy, ex1, cy + 24), Ui::warning);
                Ui::drawWrapped(smallFont, texts.emptyHelp, irr::core::rect<irr::f32>(ex0, cy + 34, ex1, listArea.LowerRightCorner.Y), Ui::textDim);
            }
            else if (hasScenario) {
                irr::f32 dy = listArea.LowerRightCorner.Y + 18;
                const irr::f32 lh = Ui::textHeight(textFont) + 8;
                detail(texts.world, widen(scenario.worldName), lx0, lx1, dy); dy += lh;
                detail(texts.ships, std::to_wstring(scenario.otherShipsData.size()), lx0, lx1, dy); dy += lh;
                wchar_t date[48];
                swprintf(date, 48, L"%02u/%02u/%04u  %ls", scenario.startDay, scenario.startMonth, scenario.startYear, timeText(scenario.startTime).c_str());
                detail(texts.start, date, lx0, lx1, dy); dy += lh + 4;
                if (!scenario.description.empty()) {
                    Ui::drawWrapped(smallFont, widen(scenario.description), irr::core::rect<irr::f32>(lx0, dy, lx1, leftCard.LowerRightCorner.Y - 16), Ui::textFaint);
                }
            }

            //Right card: station box label and hint, then who sails which ship.
            const irr::f32 rx0 = rightCard.UpperLeftCorner.X + 24, rx1 = rightCard.LowerRightCorner.X - 24;
            Ui::drawText(smallFont, texts.hostLabel, irr::core::rect<irr::f32>(rx0, hostArea.UpperLeftCorner.Y - 26, rx1, hostArea.UpperLeftCorner.Y - 6), Ui::textDim);
            if (hostBox && irr::core::stringw(hostBox->getText()).size() == 0 && !hostFocus) {
                Ui::drawText(textFont, texts.hostHint, irr::core::rect<irr::f32>(hostArea.UpperLeftCorner.X + 14, hostArea.UpperLeftCorner.Y, hostArea.LowerRightCorner.X, hostArea.LowerRightCorner.Y), Ui::textFaint);
            }
            y = assignmentTop();
            for (size_t i = 0; i < shown; i++) {
                const irr::core::rect<irr::f32> row(rightCard.UpperLeftCorner.X + 20, y, rightCard.LowerRightCorner.X - 20, y + rowH - 4);
                const irr::f32 mid = row.getCenter().X;
                std::wstring left = texts.station + L" " + std::to_wstring(i + 1);
                if (i < hosts.size()) { left += L"  \u00B7  " + hosts[i]; }
                std::wstring right;
                if (hasScenario && i < scenario.otherShipsData.size()) {
                    right = texts.ship + L" " + std::to_wstring(i + 1) + L"  \u00B7  " + widen(scenario.otherShipsData[i].shipName);
                }
                const irr::core::rect<irr::s32> leftClip = Ui::toI(irr::core::rect<irr::f32>(row.UpperLeftCorner.X, row.UpperLeftCorner.Y, mid - 22, row.LowerRightCorner.Y));
                const irr::core::rect<irr::s32> rightClip = Ui::toI(irr::core::rect<irr::f32>(mid + 18, row.UpperLeftCorner.Y, row.LowerRightCorner.X, row.LowerRightCorner.Y));
                Ui::drawText(textFont, left, irr::core::rect<irr::f32>(row.UpperLeftCorner.X + 14, row.UpperLeftCorner.Y, mid - 22, row.LowerRightCorner.Y),
                    i < hosts.size() ? Ui::text : Ui::textFaint, Ui::Left, &leftClip);
                Ui::drawText(textFont, right, irr::core::rect<irr::f32>(mid + 18, row.UpperLeftCorner.Y, row.LowerRightCorner.X - 12, row.LowerRightCorner.Y),
                    i < hosts.size() ? Ui::accentHi : Ui::textFaint, Ui::Left, &rightClip);
                y += rowH;
            }
            //What stops the start, if anything.
            std::wstring problem;
            if (hosts.empty()) { problem = texts.noHosts; }
            else if (hasScenario && hosts.size() > scenario.otherShipsData.size()) {
                problem = texts.tooMany + std::to_wstring(scenario.otherShipsData.size()) + texts.tooManyEnd;
            }
            if (!problem.empty()) {
                Ui::drawText(smallFont, problem, irr::core::rect<irr::f32>(rx0, y + 4, rx1, y + 26),
                    (attention || !hosts.empty()) ? irr::video::SColor(255, 255, 128, 128) : Ui::textDim);
            }

            //Bottom: reminder, and the start button (a GUI element).
            Ui::drawWrapped(smallFont, texts.before, irr::core::rect<irr::f32>(32, startArea.UpperLeftCorner.Y + 4, startArea.UpperLeftCorner.X - 24, H - 10), Ui::textDim);

            IGUIElement::draw();

            if (connecting) {
                driver->draw2DRectangle(irr::video::SColor(190, 2, 8, 18), irr::core::rect<irr::s32>(0, 0, (irr::s32)W, (irr::s32)H));
                const irr::core::rect<irr::f32> box(W * 0.5f - 200, H * 0.5f - 44, W * 0.5f + 200, H * 0.5f + 44);
                irr::gui::PanelBatch c;
                c.begin(driver);
                Ui::card(c, box, 14, Ui::panelTop, Ui::panelBottom);
                c.flush();
                Ui::drawText(textFont, texts.connecting, box, Ui::text, Ui::Centre);
            }
        }

    private:
        //Numbered disc before a card's title (the number is drawn with the text, in stepTitle).
        static void stepBadge(irr::gui::PanelBatch& b, const irr::core::rect<irr::f32>& card)
        {
            b.disc(irr::core::vector2df(card.UpperLeftCorner.X + 34, card.UpperLeftCorner.Y + 30), 14, Ui::primaryTop, Ui::primaryBottom);
        }

        void stepTitle(const irr::core::rect<irr::f32>& card, int n, const std::wstring& title) const
        {
            const irr::core::vector2df c(card.UpperLeftCorner.X + 34, card.UpperLeftCorner.Y + 30);
            Ui::drawText(smallFont, std::to_wstring(n), irr::core::rect<irr::f32>(c.X - 14, c.Y - 14, c.X + 14, c.Y + 14),
                irr::video::SColor(255, 255, 255, 255), Ui::Centre);
            Ui::drawText(textFont, title, irr::core::rect<irr::f32>(card.UpperLeftCorner.X + 58, card.UpperLeftCorner.Y + 16,
                card.LowerRightCorner.X - 20, card.UpperLeftCorner.Y + 44), Ui::text);
        }

        void detail(const std::wstring& label, const std::wstring& value, irr::f32 x0, irr::f32 x1, irr::f32 y) const
        {
            const irr::f32 lh = Ui::textHeight(textFont);
            Ui::drawText(textFont, label, irr::core::rect<irr::f32>(x0, y, x1, y + lh), Ui::textDim);
            Ui::drawText(textFont, value, irr::core::rect<irr::f32>(x0, y, x1, y + lh), Ui::text, Ui::Right);
        }

        irr::f32 assignmentTop() const { return hostArea.LowerRightCorner.Y + 22; }

        //One row per station typed, or per ship of the exercise if more, within the card.
        size_t assignmentRows() const
        {
            size_t n = hosts.size();
            if (hasScenario) { n = irr::core::max_(n, scenario.otherShipsData.size()); }
            const irr::f32 rowH = Ui::textHeight(textFont) + 16;
            const size_t fits = (size_t)irr::core::max_(0.0f, (rightCard.LowerRightCorner.Y - 40 - assignmentTop()) / rowH);
            return irr::core::min_(n, fits);
        }

        SetupTexts texts;
        irr::gui::IGUIFont* titleFont;
        irr::gui::IGUIFont* textFont;
        irr::gui::IGUIFont* smallFont;
        irr::core::rect<irr::f32> leftCard, rightCard, listArea, hostArea, startArea;
        irr::gui::IGUIEditBox* hostBox = 0;
        bool hasScenario, emptyList, attention, connecting;
        ScenarioData scenario;
        std::vector<std::wstring> hosts;
    };

    //Start requests: the button, a double click in the list, or Enter.
    class SetupReceiver : public irr::IEventReceiver
    {
    public:
        SetupReceiver(irr::s32 listId, irr::s32 startId) : listId(listId), startId(startId), startRequested(false) {}
        virtual bool OnEvent(const irr::SEvent& event)
        {
            if (event.EventType == irr::EET_GUI_EVENT && event.GUIEvent.Caller) {
                const irr::s32 id = event.GUIEvent.Caller->getID();
                if (event.GUIEvent.EventType == irr::gui::EGET_BUTTON_CLICKED && id == startId) { startRequested = true; }
                if (event.GUIEvent.EventType == irr::gui::EGET_LISTBOX_SELECTED_AGAIN && id == listId) { startRequested = true; }
                if (event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_ENTER) { startRequested = true; }
            }
            if (event.EventType == irr::EET_KEY_INPUT_EVENT && event.KeyInput.Key == irr::KEY_RETURN && !event.KeyInput.PressedDown) {
                startRequested = true;
            }
            return false;
        }
        irr::s32 listId, startId;
        bool startRequested;
    };
}

ScenarioChoice::ScenarioChoice(irr::IrrlichtDevice* device, Lang* language, bool french)
{
    this->language = language;
    this->device = device;
    this->french = french;
    gui = device->getGUIEnvironment();
}

void ScenarioChoice::chooseScenario(std::string& scenarioName, std::string& hostname, std::string scenarioPath)
{
	irr::video::IVideoDriver* driver = device->getVideoDriver();

    //Get list of scenarios, stored in scenarioList
    std::vector<std::string> scenarioList;
    getScenarioList(scenarioList,scenarioPath); //Populate list

    //Fonts in the sizes this screen uses, from the skin font's family.
    irr::gui::IGUIFont* textFont = gui->getSkin()->getFont();
    irr::gui::IGUIFont* titleFont = fontSizes.size() > 0 && fontSizes[0] ? fontSizes[0] : textFont;
    irr::gui::IGUIFont* smallFont = fontSizes.size() > 2 && fontSizes[2] ? fontSizes[2] : textFont;
    if (fontSizes.size() > 1 && fontSizes[1]) { textFont = fontSizes[1]; }

    const SetupTexts texts = setupTexts(french);
    SetupPanel* panel = new SetupPanel(gui, texts, titleFont, textFont, smallFont);

    gui->getSkin()->setFont(textFont); //list items in the text size
    irr::core::rect<irr::s32> listRect = panel->listRect();
    listRect.UpperLeftCorner += irr::core::position2di(6, 6); //selection band inside the rounded field
    listRect.LowerRightCorner -= irr::core::position2di(6, 6);
    irr::gui::IGUIListBox* scenarioListBox = gui->addListBox(listRect, panel, GUI_ID_SCENARIO_LISTBOX, false);
    scenarioListBox->setItemHeight((irr::s32)(Ui::textHeight(textFont) + 16));
    irr::gui::IGUIEditBox* hostnameBox = gui->addEditBox(irr::core::stringw(hostname.c_str()).c_str(), panel->hostRect(), false, panel);
    hostnameBox->setDrawBackground(false);
    hostnameBox->setOverrideColor(Ui::text);
    hostnameBox->setOverrideFont(textFont);
    panel->setHostBox(hostnameBox);
    Ui::Button* startButton = new Ui::Button(gui, panel, GUI_ID_OK_BUTTON, panel->startRect(), texts.launch.c_str(), Ui::Button::Primary);
    startButton->setFont(textFont);
    startButton->drop();

    //Add scenarios to list box (without the _mp ending that marks them as multiplayer)
    for (std::vector<std::string>::iterator it = scenarioList.begin(); it != scenarioList.end(); ++it) {
        std::string shown = *it;
        if (shown.size() > 3) { shown = shown.substr(0, shown.size() - 3); }
        scenarioListBox->addItem(irr::core::stringw(shown.c_str()).c_str());
    }
    panel->setEmptyList(scenarioList.empty());
    scenarioListBox->setVisible(!scenarioList.empty());
    //select first one if possible
    if (scenarioListBox->getItemCount()>0) {
        scenarioListBox->setSelected(0);
    }
    //select list box as active, so user can use up/down arrows without needing to click
    gui->setFocus(scenarioListBox);

    //Flush old key/clicks etc, with a 0.2s pause
    device->sleep(200);
    device->clearSystemMessages();

    //Link to our event receiver
    SetupReceiver receiver(GUI_ID_SCENARIO_LISTBOX, GUI_ID_OK_BUTTON);
    device->setEventReceiver(&receiver);

    irr::s32 shownScenario = -2;
    std::wstring shownHosts = L"\x01";
    bool chosen = false;
    while(device->run() && !chosen) {
        //Follow the selection and the station list.
        const irr::s32 selected = scenarioListBox->getSelected();
        if (selected != shownScenario) {
            shownScenario = selected;
            if (selected >= 0 && selected < (irr::s32)scenarioList.size()) {
                const ScenarioData data = Utilities::getScenarioDataFromFile(scenarioPath + scenarioList[selected], scenarioList[selected]);
                panel->setScenario(&data);
            }
            else {
                panel->setScenario(0);
            }
            panel->setAttention(false);
        }
        const std::wstring hostsNow = hostnameBox->getText();
        if (hostsNow != shownHosts) {
            shownHosts = hostsNow;
            panel->setHosts(splitHosts(hostsNow));
            panel->setAttention(false);
        }
        startButton->setEnabled(panel->ready());
        if (receiver.startRequested) {
            receiver.startRequested = false;
            if (panel->ready()) { chosen = true; }
            else { panel->setAttention(true); }
        }

        //Drawn even when the window is not the active one, so it never shows stale or blank.
        driver->beginScene(true, false, Ui::background); //Don't need to clear z buffer for 2d
        gui->drawAll();
        driver->endScene();
        device->sleep(15);
    }

    //Get name of selected scenario
    if (!chosen || scenarioListBox->getSelected() < 0 || scenarioListBox->getSelected() >= (irr::s32)scenarioList.size()) {
        exit(EXIT_FAILURE); //No scenario loaded
    }

    //Stations, cleaned up ("PC-1,PC-2"), so that what was shown is exactly what the hub connects to.
    const std::vector<std::wstring> hosts = splitHosts(hostnameBox->getText());
    std::string sHostname;
    for (size_t i = 0; i < hosts.size(); i++) {
        if (i) { sHostname += ","; }
        sHostname += std::string(hosts[i].begin(), hosts[i].end());
    }
    hostname = sHostname; //hostname is a pass by reference return value

    scenarioName = scenarioList[scenarioListBox->getSelected()]; //scenarioName is a pass by reference return value

    //Connecting can take a moment: say so on the last frame of this screen.
    panel->setConnecting(true);
    driver->beginScene(true, false, Ui::background);
    gui->drawAll();
    driver->endScene();

    //Clean up
    device->setEventReceiver(0); //Remove link to the event receiver, as this will be destroyed.
    panel->remove();
    panel->drop();
}

void ScenarioChoice::setFonts(irr::gui::IGUIFont* title, irr::gui::IGUIFont* text, irr::gui::IGUIFont* small)
{
    fontSizes.clear();
    fontSizes.push_back(title);
    fontSizes.push_back(text);
    fontSizes.push_back(small);
}

void ScenarioChoice::getScenarioList(std::vector<std::string>&scenarioList, std::string scenarioPath) {

	irr::io::IFileSystem* fileSystem = device->getFileSystem();
    if (fileSystem==0) {
        exit(EXIT_FAILURE); //Could not get file system TODO: Message for user
    }
    //store current dir
    irr::io::path cwd = fileSystem->getWorkingDirectory();

    //change to scenario dir
    if (!fileSystem->changeWorkingDirectoryTo(scenarioPath.c_str())) {
        exit(EXIT_FAILURE); //Couldn't change to scenario dir
    }

    irr::io::IFileList* fileList = fileSystem->createFileList();
    if (fileList==0) {
        exit(EXIT_FAILURE); //Could not get file list for scenarios TODO: Message for user
    }

    //List here
    for (irr::u32 i=0;i<fileList->getFileCount();i++) {
        if (fileList->isDirectory(i)) {
            const irr::io::path& fileName = fileList->getFileName(i);
            if (fileName.findFirst('.')!=0) { //Check it doesn't start with '.' (., .., or hidden)
                //std::cout << fileName.c_str() << std::endl;

                //Check if name ends with "_mp" for multiplayer:
                if (fileName.size() >= 3) {
                    const irr::io::path endChars = fileName.subString(fileName.size()-3,3,true);
                    if (endChars == irr::io::path("_mp")) {
                        scenarioList.push_back(fileName.c_str());
                    }
                }

            }
        }
    }

    //change back
    if (!fileSystem->changeWorkingDirectoryTo(cwd)) {
        exit(EXIT_FAILURE); //Couldn't change dir back
    }
    fileList->drop();
}
