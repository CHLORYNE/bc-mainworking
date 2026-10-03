/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2015 James Packer

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

#include "GUI.hpp"
#include "../Constants.hpp"
#include "../Utilities.hpp"
#include "../chartView/ChartView.hpp"
#include "../chartView/ChartDraw.hpp"
#include "../IniFile.hpp"
#include "../UiTheme.hpp"

#include <iostream>
#include <limits>
#include <string>
#include <algorithm>

//using namespace irr;

namespace {
    //Language of the NAUTITECH tools: the simulator's (bc5.ini lang). French unless it is set to something else.
    bool productFrench()
    {
        std::string ini = "bc5.ini";
        if (Utilities::pathExists(Utilities::getUserDir() + ini)) { ini = Utilities::getUserDir() + ini; }
        const std::string lang = IniFile::iniFileToString(ini, "lang");
        return lang.empty() || lang == "fr";
    }

    //The editor's font (map.ini font and font_scale) at a given size.
    irr::gui::IGUIFont* editorFont(irr::gui::IGUIEnvironment* env, int size)
    {
        std::string ini = "map.ini";
        if (Utilities::pathExists(Utilities::getUserDir() + ini)) { ini = Utilities::getUserDir() + ini; }
        std::string name = IniFile::iniFileToString(ini, "font");
        if (name.empty()) { name = "noto-sans"; }
        irr::f32 scale = IniFile::iniFileTof32(ini, "font_scale");
        if (scale < 1) { scale = 1; }
        size = irr::core::min_((int)(size * scale + 0.5f), 36);
        irr::gui::IGUIFont* f = env->getFont(("media/fonts/" + name + "/" + name + "-" + std::to_string(size) + ".xml").c_str());
        return f ? f : env->getSkin()->getFont();
    }

    //Side panel background: header (badge, title, area), tab bar, and the bar with Apply / Save.
    class SidePanel : public irr::gui::IGUIElement
    {
    public:
        SidePanel(irr::gui::IGUIEnvironment* env, const irr::core::rect<irr::s32>& r, const std::wstring& title,
            irr::gui::IGUIFont* titleFont, irr::gui::IGUIFont* smallFont, irr::s32 tabsBottom, irr::s32 footerTop)
            : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, env, env->getRootGUIElement(), -1, r),
            title(title), titleFont(titleFont), smallFont(smallFont), tabsBottom(tabsBottom), footerTop(footerTop)
        {
        }
        void setSubtitle(const std::wstring& s) { subtitle = s; }

        virtual void draw()
        {
            if (!IsVisible) { return; }
            const irr::core::rect<irr::f32> r = Ui::toF(AbsoluteRect);
            irr::video::IVideoDriver* driver = Environment->getVideoDriver();
            driver->draw2DRectangle(AbsoluteRect, Ui::panelTop, Ui::panelTop, Ui::background, Ui::background);
            irr::gui::PanelBatch b;
            b.begin(driver);
            b.rect(irr::core::rect<irr::f32>(r.LowerRightCorner.X - 1, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y), Ui::edge);
            const irr::core::vector2df badge(r.UpperLeftCorner.X + 38, r.UpperLeftCorner.Y + 38);
            b.disc(badge, 20, irr::video::SColor(70, 64, 156, 240), irr::video::SColor(70, 64, 156, 240));
            //Route icon: track with waypoints
            const irr::f32 s = 10;
            const irr::core::vector2df p[4] = { irr::core::vector2df(badge.X - s * 0.8f, badge.Y + s * 0.62f), irr::core::vector2df(badge.X - s * 0.2f, badge.Y + s * 0.05f),
                irr::core::vector2df(badge.X + s * 0.28f, badge.Y + s * 0.38f), irr::core::vector2df(badge.X + s * 0.78f, badge.Y - s * 0.55f) };
            for (int i = 0; i < 3; i++) { b.line(p[i], p[i + 1], 1.8f, Ui::accentHi); }
            for (int i = 0; i < 3; i++) { b.disc(p[i], 2.2f, Ui::accentHi, Ui::accentHi); }
            b.sector(p[3], 1.8f, 3.6f, 0, 360, Ui::accentHi, Ui::accentHi);
            b.rect(irr::core::rect<irr::f32>(r.UpperLeftCorner.X + 18, r.UpperLeftCorner.Y + tabsBottom + 10, r.LowerRightCorner.X - 18, r.UpperLeftCorner.Y + tabsBottom + 11), Ui::rule);
            b.rect(irr::core::rect<irr::f32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y + footerTop, r.LowerRightCorner.X - 1, r.LowerRightCorner.Y), irr::video::SColor(255, 9, 18, 33));
            b.rect(irr::core::rect<irr::f32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y + footerTop, r.LowerRightCorner.X - 1, r.UpperLeftCorner.Y + footerTop + 1), Ui::edge);
            b.flush();
            Ui::drawText(titleFont, title, irr::core::rect<irr::f32>(r.UpperLeftCorner.X + 68, r.UpperLeftCorner.Y + 16, r.LowerRightCorner.X - 100, r.UpperLeftCorner.Y + 42), Ui::text);
            Ui::drawText(smallFont, subtitle, irr::core::rect<irr::f32>(r.UpperLeftCorner.X + 68, r.UpperLeftCorner.Y + 42, r.LowerRightCorner.X - 100, r.UpperLeftCorner.Y + 62), Ui::textDim);
            IGUIElement::draw();
        }

    private:
        std::wstring title, subtitle;
        irr::gui::IGUIFont* titleFont;
        irr::gui::IGUIFont* smallFont;
        irr::s32 tabsBottom, footerTop;
    };

    //Page of the side panel: cards with a title, labels, and a rounded field behind each edit box and
    //list (they draw no frame of their own), then its controls.
    class SidePage : public irr::gui::IGUIElement
    {
    public:
        SidePage(irr::gui::IGUIEnvironment* env, irr::gui::IGUIElement* parent, const irr::core::rect<irr::s32>& r,
            irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* smallFont)
            : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, env, parent, -1, r), textFont(textFont), smallFont(smallFont)
        {
        }

        void addCard(const irr::core::rect<irr::s32>& r, const std::wstring& title) { cards.push_back(Item(r, title)); }
        void addLabel(const irr::core::rect<irr::s32>& r, const std::wstring& text) { labels.push_back(Item(r, text)); }

        virtual void draw()
        {
            if (!IsVisible) { return; }
            const irr::core::position2di o = AbsoluteRect.UpperLeftCorner;
            irr::gui::PanelBatch b;
            b.begin(Environment->getVideoDriver());
            for (size_t i = 0; i < cards.size(); i++) {
                Ui::card(b, Ui::toF(cards[i].r + o), 12);
            }
            //Fields behind edit boxes and lists
            for (irr::core::list<irr::gui::IGUIElement*>::Iterator it = Children.begin(); it != Children.end(); ++it) {
                irr::gui::IGUIElement* e = *it;
                if (!e->isVisible()) { continue; }
                const irr::gui::EGUI_ELEMENT_TYPE t = e->getType();
                if (t != irr::gui::EGUIET_EDIT_BOX && t != irr::gui::EGUIET_LIST_BOX) { continue; }
                irr::core::rect<irr::f32> f = Ui::toF(e->getAbsolutePosition());
                if (t == irr::gui::EGUIET_EDIT_BOX) { f.UpperLeftCorner.X -= 8; f.LowerRightCorner.X += 8; }
                const bool focus = Environment->hasFocus(e);
                Ui::roundRect(b, f, 7, Ui::field, Ui::field);
                Ui::roundRectOutline(b, f, 7, 1.0f, focus ? Ui::accentHi : irr::video::SColor(120, 110, 160, 220));
            }
            b.flush();
            for (size_t i = 0; i < cards.size(); i++) {
                const irr::core::rect<irr::s32> c = cards[i].r + o;
                Ui::drawText(textFont, cards[i].text, irr::core::rect<irr::f32>((irr::f32)c.UpperLeftCorner.X + 14, (irr::f32)c.UpperLeftCorner.Y + 8,
                    (irr::f32)c.LowerRightCorner.X - 14, (irr::f32)c.UpperLeftCorner.Y + 34), Ui::accentHi);
            }
            for (size_t i = 0; i < labels.size(); i++) {
                Ui::drawText(smallFont, labels[i].text, Ui::toF(labels[i].r + o), Ui::textDim);
            }
            IGUIElement::draw();
        }

    private:
        struct Item {
            irr::core::rect<irr::s32> r;
            std::wstring text;
            Item(const irr::core::rect<irr::s32>& r, const std::wstring& t) : r(r), text(t) {}
        };
        irr::gui::IGUIFont* textFont;
        irr::gui::IGUIFont* smallFont;
        std::vector<Item> cards, labels;
    };

    //An edit box without its own frame (the page draws the field), in the panel's text colour.
    irr::gui::IGUIEditBox* fieldEdit(irr::gui::IGUIEnvironment* env, irr::gui::IGUIElement* page, const irr::core::rect<irr::s32>& r, irr::s32 id,
        irr::gui::IGUIFont* font)
    {
        irr::gui::IGUIEditBox* e = env->addEditBox(L"", irr::core::rect<irr::s32>(r.UpperLeftCorner.X + 8, r.UpperLeftCorner.Y, r.LowerRightCorner.X - 8, r.LowerRightCorner.Y), false, page, id);
        e->setDrawBorder(false);
        e->setDrawBackground(false);
        e->setOverrideColor(Ui::text);
        e->setOverrideFont(font);
        return e;
    }

    Ui::Button* panelButton(irr::gui::IGUIEnvironment* env, irr::gui::IGUIElement* parent, const irr::core::rect<irr::s32>& r, irr::s32 id,
        const std::wstring& label, Ui::Button::Kind kind, irr::gui::IGUIFont* font)
    {
        Ui::Button* b = new Ui::Button(env, parent, id, r, label.c_str(), kind);
        b->setFont(font);
        b->drop(); //the GUI tree holds it
        return b;
    }

    irr::gui::IGUIStaticText* noteText(irr::gui::IGUIEnvironment* env, irr::gui::IGUIElement* page, const irr::core::rect<irr::s32>& r,
        const std::wstring& text, irr::video::SColor colour, irr::gui::IGUIFont* font)
    {
        irr::gui::IGUIStaticText* t = env->addStaticText(text.c_str(), r, false, true, page);
        t->setOverrideColor(colour);
        t->setOverrideFont(font);
        return t;
    }
}

GUIMain::GUIMain(irr::IrrlichtDevice* device, Lang* language, std::vector<std::string> ownShipTypes, std::vector<std::string> otherShipTypes, bool multiplayer)
{
    this->device = device;
    this->language = language;
    this->multiplayer = multiplayer;
    guienv = device->getGUIEnvironment();
    returnToMenuFlag = false;
    french = productFrench();

    irr::video::IVideoDriver* driver = device->getVideoDriver();
    const irr::s32 W = (irr::s32)driver->getScreenSize().Width;
    const irr::s32 H = (irr::s32)driver->getScreenSize().Height;

    //Look: the launcher's navy theme. Controls use a slightly larger font than before; the chart keeps
    //the smaller one for its labels.
    irr::gui::IGUISkin* skin = guienv->getSkin();
    originalSkinFont = skin->getFont();
    mapFont = originalSkinFont;
    titleFont = editorFont(guienv, 19);
    textFont = editorFont(guienv, 15);
    smallFont = editorFont(guienv, 13);
    Ui::applySkin(skin);
    skin->setColor(irr::gui::EGDC_3D_HIGH_LIGHT, Ui::field);                       //combo and list backgrounds
    skin->setColor(irr::gui::EGDC_3D_SHADOW, irr::video::SColor(255, 40, 64, 98));  //their borders
    skin->setColor(irr::gui::EGDC_3D_DARK_SHADOW, irr::video::SColor(255, 40, 64, 98));
    skin->setFont(textFont);

    //Side panel geometry
    sidebarWidth = irr::core::clamp((irr::s32)(W * 0.31f), 360, 470);
    sidebarShown = true;
    activeTab = 0;
    const irr::s32 S = sidebarWidth;
    const irr::s32 ch = (irr::s32)Ui::textHeight(textFont) + 14;  //control height
    const irr::s32 lh = (irr::s32)Ui::textHeight(smallFont) + 6;  //label height
    const irr::s32 tabsTop = 82, tabsBottom = tabsTop + 38;
    const irr::s32 footerTop = H - 66;
    const irr::s32 contentTop = tabsBottom + 22;
    const irr::s32 x0 = 18, x1 = S - 18;          //cards
    const irr::s32 cx0 = x0 + 14, cx1 = x1 - 14;  //inside cards
    const irr::s32 mid = (cx0 + cx1) / 2;

    SidePanel* panel = new SidePanel(guienv, irr::core::rect<irr::s32>(0, 0, S, H),
        french ? L"\u00C9diteur d'exercices" : L"Exercise editor", titleFont, smallFont, tabsBottom, footerTop);
    panel->drop();
    sidebar = panel;

    backButton = panelButton(guienv, sidebar, irr::core::rect<irr::s32>(x1 - 86, 22, x1, 22 + 34), GUI_ID_BACK_BUTTON,
        french ? L"Menu" : L"Menu", Ui::Button::Secondary, textFont);
    backButton->setToolTipText(french ? L"Retour \u00E0 l'\u00E9cran de choix (sans enregistrer)" : L"Back to the start screen (without saving)");

    //Tabs
    const wchar_t* tabNames[4] = { french ? L"Exercice" : L"Exercise", french ? L"Navires" : L"Ships",
        french ? L"Route" : L"Route", french ? L"M\u00E9t\u00E9o" : L"Weather" };
    const irr::s32 tabW = (x1 - x0 - 3 * 6) / 4;
    for (int i = 0; i < 4; i++) {
        const irr::s32 tx = x0 + i * (tabW + 6);
        tabButtons[i] = panelButton(guienv, sidebar, irr::core::rect<irr::s32>(tx, tabsTop, tx + tabW, tabsBottom), GUI_ID_TAB_EXERCISE + i,
            tabNames[i], Ui::Button::Secondary, textFont);
    }

    //Apply / Save
    apply = panelButton(guienv, sidebar, irr::core::rect<irr::s32>(x0, footerTop + 14, x0 + 130, footerTop + 14 + 38), GUI_ID_APPLY_BUTTON,
        french ? L"Appliquer" : L"Apply", Ui::Button::Secondary, textFont);
    apply->setToolTipText(french ? L"Prendre en compte l'exercice et la m\u00E9t\u00E9o sans enregistrer" : L"Use the exercise and weather settings without saving");
    save = panelButton(guienv, sidebar, irr::core::rect<irr::s32>(x0 + 142, footerTop + 14, x1, footerTop + 14 + 38), GUI_ID_SAVE_BUTTON,
        french ? L"Enregistrer l'exercice" : L"Save the exercise", Ui::Button::Primary, textFont);

    //Pages (and the ship card shared by Ships and Route), below the tabs
    const irr::s32 contentH = footerTop - 12 - contentTop;
    SidePage* page[4];
    for (int i = 0; i < 4; i++) {
        page[i] = new SidePage(guienv, sidebar, irr::core::rect<irr::s32>(0, contentTop, S, contentTop + contentH), textFont, smallFont);
        page[i]->drop();
        pages[i] = page[i];
    }
    //Ship card: which ship, add / delete, place at the chart centre
    const irr::s32 stripH = 40 + ch + 8 + ch + 6 + lh + 12;
    SidePage* strip = new SidePage(guienv, sidebar, irr::core::rect<irr::s32>(0, contentTop, S, contentTop + stripH), textFont, smallFont);
    strip->drop();
    shipStrip = strip;
    {
        strip->addCard(irr::core::rect<irr::s32>(x0, 0, x1, stripH - 2), french ? L"Navire" : L"Ship");
        irr::s32 y = 40;
        shipSelector = guienv->addComboBox(irr::core::rect<irr::s32>(cx0, y, cx1 - 112, y + ch), strip, GUI_ID_SHIP_COMBOBOX);
        deleteShip = panelButton(guienv, strip, irr::core::rect<irr::s32>(cx1 - 104, y, cx1, y + ch), GUI_ID_DELETESHIP_BUTTON,
            french ? L"Supprimer" : L"Delete", Ui::Button::Danger, textFont);
        deleteShip->setToolTipText(french ? L"Supprimer le navire choisi (pas le navire propre)" : L"Delete the chosen ship (not the own ship)");
        y += ch + 8;
        addShip = panelButton(guienv, strip, irr::core::rect<irr::s32>(cx0, y, mid - 4, y + ch), GUI_ID_ADDSHIP_BUTTON,
            french ? L"+ Nouveau navire" : L"+ New ship", Ui::Button::Secondary, textFont);
        addShip->setToolTipText(french ? L"Ajoute un navire au centre de la carte (rep\u00E8re +), du mod\u00E8le choisi" : L"Adds a ship at the chart centre (+ mark), of the chosen model");
        moveShip = panelButton(guienv, strip, irr::core::rect<irr::s32>(mid + 4, y, cx1, y + ch), GUI_ID_MOVESHIP_BUTTON,
            french ? L"Placer au centre" : L"Move to centre", Ui::Button::Secondary, textFont);
        moveShip->setToolTipText(french ? L"Place le navire choisi au centre de la carte (rep\u00E8re +)" : L"Moves the chosen ship to the chart centre (+ mark)");
        y += ch + 6;
        dataDisplay = noteText(guienv, strip, irr::core::rect<irr::s32>(cx0, y, cx1, y + lh), L"", Ui::textFaint, smallFont);
    }
    const irr::s32 belowStrip = stripH + 12;

    //--- Exercise page ---
    {
        SidePage* p = page[0];
        irr::s32 y = 0;
        const irr::s32 cardA = 40 + lh + ch + 4 + 2 * lh + 6 + lh + 3 * ch + 14;
        p->addCard(irr::core::rect<irr::s32>(x0, y, x1, y + cardA), french ? L"Exercice" : L"Exercise");
        y += 40;
        p->addLabel(irr::core::rect<irr::s32>(cx0, y, cx1, y + lh), french ? L"Nom de l'exercice" : L"Exercise name");
        y += lh;
        scenarioName = fieldEdit(guienv, p, irr::core::rect<irr::s32>(cx0, y, cx1, y + ch), GUI_ID_SCENARIONAME_EDITBOX, textFont);
        y += ch + 4;
        overwriteWarning = noteText(guienv, p, irr::core::rect<irr::s32>(cx0, y, cx1, y + 2 * lh),
            french ? L"Un exercice porte d\u00E9j\u00E0 ce nom : il sera remplac\u00E9 \u00E0 l'enregistrement." : L"An exercise already has this name: it will be replaced when saved.",
            irr::video::SColor(255, 255, 150, 120), smallFont);
        multiplayerNameWarning = noteText(guienv, p, irr::core::rect<irr::s32>(cx0, y, cx1, y + 2 * lh),
            french ? L"Exercice multijoueur : le nom doit se terminer par _mp." : L"Multiplayer exercise: the name must end with _mp.",
            Ui::warning, smallFont);
        notMultiplayerNameWarning = noteText(guienv, p, irr::core::rect<irr::s32>(cx0, y, cx1, y + 2 * lh),
            french ? L"Exercice simple : le nom ne doit pas se terminer par _mp." : L"Single-station exercise: the name must not end with _mp.",
            Ui::warning, smallFont);
        y += 2 * lh + 6;
        p->addLabel(irr::core::rect<irr::s32>(cx0, y, cx1, y + lh), french ? L"Description (affich\u00E9e au choix de l'exercice)" : L"Description (shown when choosing the exercise)");
        y += lh;
        descriptionEdit = fieldEdit(guienv, p, irr::core::rect<irr::s32>(cx0, y + 4, cx1, y + 3 * ch - 4), GUI_ID_DESCRIPTION_EDITBOX, textFont);
        descriptionEdit->setMultiLine(true);
        descriptionEdit->setWordWrap(true);
        descriptionEdit->setAutoScroll(true);
        descriptionEdit->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_UPPERLEFT);

        y = cardA + 12;
        const irr::s32 cardB = 40 + 2 * (lh + ch) + 8 + 14;
        p->addCard(irr::core::rect<irr::s32>(x0, y, x1, y + cardB), french ? L"D\u00E9part" : L"Start");
        y += 40;
        //Date fields, then the time just after them (or at mid width if there is room).
        const irr::s32 tx = irr::core::max_(mid + 16, cx0 + 168 + 20);
        p->addLabel(irr::core::rect<irr::s32>(cx0, y, tx - 8, y + lh), french ? L"Date (JJ / MM / AAAA)" : L"Date (DD / MM / YYYY)");
        p->addLabel(irr::core::rect<irr::s32>(tx, y, cx1, y + lh), french ? L"Heure (HH : MM)" : L"Time (HH : MM)");
        y += lh;
        startDay = fieldEdit(guienv, p, irr::core::rect<irr::s32>(cx0, y, cx0 + 44, y + ch), GUI_ID_STARTDAY_EDITBOX, textFont);
        startMonth = fieldEdit(guienv, p, irr::core::rect<irr::s32>(cx0 + 54, y, cx0 + 98, y + ch), GUI_ID_STARTMONTH_EDITBOX, textFont);
        startYear = fieldEdit(guienv, p, irr::core::rect<irr::s32>(cx0 + 108, y, cx0 + 168, y + ch), GUI_ID_STARTYEAR_EDITBOX, textFont);
        startHours = fieldEdit(guienv, p, irr::core::rect<irr::s32>(tx, y, tx + 44, y + ch), GUI_ID_STARTHOURS_EDITBOX, textFont);
        startMins = fieldEdit(guienv, p, irr::core::rect<irr::s32>(tx + 54, y, tx + 98, y + ch), GUI_ID_STARTMINS_EDITBOX, textFont);
        y += ch + 8;
        p->addLabel(irr::core::rect<irr::s32>(cx0, y, mid, y + lh), french ? L"Lever du soleil (h)" : L"Sunrise (h)");
        p->addLabel(irr::core::rect<irr::s32>(tx, y, cx1, y + lh), french ? L"Coucher du soleil (h)" : L"Sunset (h)");
        y += lh;
        sunRise = fieldEdit(guienv, p, irr::core::rect<irr::s32>(cx0, y, cx0 + 90, y + ch), GUI_ID_SUNRISE_EDITBOX, textFont);
        sunSet = fieldEdit(guienv, p, irr::core::rect<irr::s32>(tx, y, tx + 90, y + ch), GUI_ID_SUNSET_EDITBOX, textFont);
    }

    //--- Ships page: model with its picture, identification ---
    {
        SidePage* p = page[1];
        irr::s32 y = belowStrip;
        //Picture as large as the space allows (4:3 at most, none if there is no room).
        const irr::s32 identityH = 40 + lh + ch + 10 + ch + 14;
        const irr::s32 roomForPicture = contentH - belowStrip - (40 + ch + 10 + 14) - 12 - identityH;
        const irr::s32 pictureH = irr::core::clamp(roomForPicture, 0, (cx1 - cx0) * 9 / 16);
        const irr::s32 modelH = 40 + ch + (pictureH > 40 ? 10 + pictureH : 0) + 14;
        p->addCard(irr::core::rect<irr::s32>(x0, y, x1, y + modelH), french ? L"Mod\u00E8le" : L"Model");
        y += 40;
        ownShipTypeSelector = guienv->addComboBox(irr::core::rect<irr::s32>(cx0, y, cx1, y + ch), p, GUI_ID_OWNSHIPSELECT_COMBOBOX);
        for (int i = 0; i < ownShipTypes.size(); i++) {
            ownShipTypeSelector->addItem(irr::core::stringw(ownShipTypes.at(i).c_str()).c_str());
        }
        otherShipTypeSelector = guienv->addComboBox(irr::core::rect<irr::s32>(cx0, y, cx1, y + ch), p, GUI_ID_OTHERSHIPSELECT_COMBOBOX);
        for (int i = 0; i < otherShipTypes.size(); i++) {
            otherShipTypeSelector->addItem(irr::core::stringw(otherShipTypes.at(i).c_str()).c_str());
        }
        otherShipTypeSelector->setVisible(false); //Initially show own ship selector.
        y += ch + 10;
        const irr::core::rect<irr::s32> imageRect(cx0, y, cx1, y + irr::core::max_(pictureH, 1));
        shipImageDisplay = guienv->addImage(imageRect, p);
        shipImageDisplay->setScaleImage(true);
        shipImageDisplay->setVisible(false);
        shipImageNotFoundText = guienv->addStaticText(french ? L"Pas de photo pour ce mod\u00E8le" : L"No picture for this model", imageRect, false, true, p, -1, true);
        shipImageNotFoundText->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
        shipImageNotFoundText->setBackgroundColor(irr::video::SColor(255, 10, 20, 36));
        shipImageNotFoundText->setOverrideColor(Ui::textFaint);
        shipImageNotFoundText->setOverrideFont(smallFont);
        shipImageNotFoundText->setVisible(false);
        if (pictureH <= 40) { shipImageDisplay->setEnabled(false); } //no room: never shown
        hasValidImage = false;

        y = belowStrip + modelH + 12;
        p->addCard(irr::core::rect<irr::s32>(x0, y, x1, y + identityH), french ? L"Identification" : L"Identification");
        y += 40;
        p->addLabel(irr::core::rect<irr::s32>(cx0, y, cx1, y + lh), L"MMSI");
        y += lh;
        mmsiEdit = fieldEdit(guienv, p, irr::core::rect<irr::s32>(cx0, y, cx1 - 112, y + ch), GUI_ID_MMSI_EDITBOX, textFont);
        setMMSI = panelButton(guienv, p, irr::core::rect<irr::s32>(cx1 - 104, y, cx1, y + ch), GUI_ID_SETMMSI_BUTTON,
            french ? L"Appliquer" : L"Apply", Ui::Button::Secondary, textFont);
        y += ch + 10;
        isDrifting = guienv->addCheckBox(false, irr::core::rect<irr::s32>(cx0, y, cx1, y + ch), p, GUI_ID_DRIFTING_CHECKBOX,
            french ? L"D\u00E9rive libre (vent et courant)" : L"Free drift (wind and current)");
    }

    //--- Route page: legs of the chosen ship ---
    {
        SidePage* p = page[2];
        irr::s32 y = belowStrip;
        const irr::s32 fixed = 40 + 2 * lh + 8 + 10 + lh + ch + 12 + ch + 14;
        const irr::s32 listH = irr::core::clamp(contentH - belowStrip - fixed, 3 * (ch - 6), 8 * (ch - 6));
        p->addCard(irr::core::rect<irr::s32>(x0, y, x1, y + fixed + listH), french ? L"Route" : L"Route");
        y += 40;
        routeHint = noteText(guienv, p, irr::core::rect<irr::s32>(cx0, y, cx1, y + 2 * lh), L"", Ui::textDim, smallFont);
        y += 2 * lh + 8;
        legSelector = guienv->addListBox(irr::core::rect<irr::s32>(cx0, y, cx1, y + listH), p, GUI_ID_LEG_LISTBOX, false);
        legSelector->setItemHeight(ch - 6);
        y += listH + 10;
        const irr::s32 colW = (cx1 - cx0 - 2 * 10) / 3;
        p->addLabel(irr::core::rect<irr::s32>(cx0, y, cx0 + colW, y + lh), french ? L"Cap (\u00B0)" : L"Course (\u00B0)");
        p->addLabel(irr::core::rect<irr::s32>(cx0 + colW + 10, y, cx0 + 2 * colW + 10, y + lh), french ? L"Vitesse (nds)" : L"Speed (kn)");
        p->addLabel(irr::core::rect<irr::s32>(cx0 + 2 * colW + 20, y, cx1, y + lh), french ? L"Distance (NM)" : L"Distance (NM)");
        y += lh;
        legCourseEdit = fieldEdit(guienv, p, irr::core::rect<irr::s32>(cx0, y, cx0 + colW, y + ch), GUI_ID_COURSE_EDITBOX, textFont);
        legSpeedEdit = fieldEdit(guienv, p, irr::core::rect<irr::s32>(cx0 + colW + 10, y, cx0 + 2 * colW + 10, y + ch), GUI_ID_SPEED_EDITBOX, textFont);
        legDistanceEdit = fieldEdit(guienv, p, irr::core::rect<irr::s32>(cx0 + 2 * colW + 20, y, cx1, y + ch), GUI_ID_DISTANCE_EDITBOX, textFont);
        y += ch + 12;
        changeLeg = panelButton(guienv, p, irr::core::rect<irr::s32>(cx0, y, cx0 + colW, y + ch), GUI_ID_CHANGE_BUTTON,
            french ? L"Modifier" : L"Change", Ui::Button::Primary, textFont);
        changeLeg->setToolTipText(french ? L"Applique cap, vitesse et distance au segment choisi" : L"Applies course, speed and distance to the chosen leg");
        addLeg = panelButton(guienv, p, irr::core::rect<irr::s32>(cx0 + colW + 10, y, cx0 + 2 * colW + 10, y + ch), GUI_ID_ADDLEG_BUTTON,
            french ? L"Ins\u00E9rer apr\u00E8s" : L"Insert after", Ui::Button::Secondary, textFont);
        addLeg->setToolTipText(french ? L"Ajoute un segment apr\u00E8s celui choisi, avec ces valeurs" : L"Adds a leg after the chosen one, with these values");
        deleteLeg = panelButton(guienv, p, irr::core::rect<irr::s32>(cx0 + 2 * colW + 20, y, cx1, y + ch), GUI_ID_DELETELEG_BUTTON,
            french ? L"Supprimer" : L"Delete", Ui::Button::Danger, textFont);
    }

    //--- Weather page ---
    {
        SidePage* p = page[3];
        irr::s32 y = 0;
        const irr::s32 windH = 40 + lh + ch + 14;
        p->addCard(irr::core::rect<irr::s32>(x0, y, x1, y + windH), french ? L"Vent" : L"Wind");
        y += 40;
        p->addLabel(irr::core::rect<irr::s32>(cx0, y, mid, y + lh), french ? L"Direction (\u00B0, d'o\u00F9 il vient)" : L"Direction (\u00B0, from)");
        p->addLabel(irr::core::rect<irr::s32>(mid + 16, y, cx1, y + lh), french ? L"Vitesse (nds)" : L"Speed (kn)");
        y += lh;
        windDirection = fieldEdit(guienv, p, irr::core::rect<irr::s32>(cx0, y, cx0 + 110, y + ch), GUI_ID_WINDDIRECTION_EDITBOX, textFont);
        windSpeed = fieldEdit(guienv, p, irr::core::rect<irr::s32>(mid + 16, y, mid + 126, y + ch), GUI_ID_WINDSPEED_EDITBOX, textFont);

        y = windH + 12;
        const irr::s32 seaH = 40 + 3 * (lh + ch) + 2 * 8 + 14;
        p->addCard(irr::core::rect<irr::s32>(x0, y, x1, y + seaH), french ? L"Mer et visibilit\u00E9" : L"Sea and visibility");
        y += 40;
        p->addLabel(irr::core::rect<irr::s32>(cx0, y, cx1, y + lh), french ? L"\u00C9tat de la mer (0 \u00E0 12)" : L"Sea state (0 to 12)");
        y += lh;
        weather = guienv->addComboBox(irr::core::rect<irr::s32>(cx0, y, cx0 + 140, y + ch), p, GUI_ID_WEATHER_COMBOBOX);
        y += ch + 8;
        p->addLabel(irr::core::rect<irr::s32>(cx0, y, cx1, y + lh), french ? L"Pluie (0 \u00E0 10)" : L"Rain (0 to 10)");
        y += lh;
        rain = guienv->addComboBox(irr::core::rect<irr::s32>(cx0, y, cx0 + 140, y + ch), p, GUI_ID_RAIN_COMBOBOX);
        y += ch + 8;
        p->addLabel(irr::core::rect<irr::s32>(cx0, y, cx1, y + lh), french ? L"Visibilit\u00E9 (NM)" : L"Visibility (NM)");
        y += lh;
        visibility = guienv->addComboBox(irr::core::rect<irr::s32>(cx0, y, cx0 + 140, y + ch), p, GUI_ID_VISIBILITY_COMBOBOX);
    }

    //Map toolbar: chart background and zoom at the top right, the panel toggle beside the panel.
    chartStyleButton = panelButton(guienv, 0, irr::core::rect<irr::s32>(W - 14 - 2 * 40 - 8 - 250, 14, W - 14 - 2 * 40 - 8, 14 + 36), GUI_ID_CHARTSTYLE_BUTTON,
        french ? L"Fond" : L"Background", Ui::Button::Secondary, textFont);
    chartStyleButton->setToolTipText(french ? L"Fond de carte : carte marine jour / nuit, carte d'origine, image" : L"Chart background: nautical chart day / night, original map, image");
    zoomOut = panelButton(guienv, 0, irr::core::rect<irr::s32>(W - 14 - 2 * 40 - 4 + 4, 14, W - 14 - 40 - 4, 14 + 36), GUI_ID_ZOOMOUT_BUTTON,
        L"", Ui::Button::Secondary, titleFont);
    static_cast<Ui::Button*>(zoomOut)->setGlyph(Ui::Button::Minus);
    zoomIn = panelButton(guienv, 0, irr::core::rect<irr::s32>(W - 14 - 36, 14, W - 14, 14 + 36), GUI_ID_ZOOMIN_BUTTON,
        L"", Ui::Button::Secondary, titleFont);
    static_cast<Ui::Button*>(zoomIn)->setGlyph(Ui::Button::Plus);
    zoomOut->setToolTipText(french ? L"Zoom arri\u00E8re (molette)" : L"Zoom out (wheel)");
    zoomIn->setToolTipText(french ? L"Zoom avant (molette)" : L"Zoom in (wheel)");
    toggleUIButton = panelButton(guienv, 0, irr::core::rect<irr::s32>(S + 12, 14, S + 12 + 40, 14 + 36), GUI_ID_TOGGLE_UI_BUTTON,
        L"", Ui::Button::Secondary, titleFont);
    static_cast<Ui::Button*>(toggleUIButton)->setGlyph(Ui::Button::ChevronLeft);
    toggleUIButton->setToolTipText(french ? L"Masquer / afficher le panneau" : L"Hide / show the panel");

    weather->addItem(L"0"); weather->addItem(L"0.5"); weather->addItem(L"1"); weather->addItem(L"1.5");
    weather->addItem(L"2"); weather->addItem(L"2.5"); weather->addItem(L"3"); weather->addItem(L"3.5");
    weather->addItem(L"4"); weather->addItem(L"4.5"); weather->addItem(L"5"); weather->addItem(L"5.5");
    weather->addItem(L"6"); weather->addItem(L"6.5"); weather->addItem(L"7"); weather->addItem(L"7.5");
    weather->addItem(L"8"); weather->addItem(L"8.5"); weather->addItem(L"9"); weather->addItem(L"9.5");
    weather->addItem(L"10"); weather->addItem(L"10.5"); weather->addItem(L"11"); weather->addItem(L"11.5");
    weather->addItem(L"12");

    rain->addItem(L"0"); rain->addItem(L"0.5"); rain->addItem(L"1"); rain->addItem(L"1.5");
    rain->addItem(L"2"); rain->addItem(L"2.5"); rain->addItem(L"3"); rain->addItem(L"3.5");
    rain->addItem(L"4"); rain->addItem(L"4.5"); rain->addItem(L"5"); rain->addItem(L"5.5");
    rain->addItem(L"6"); rain->addItem(L"6.5"); rain->addItem(L"7"); rain->addItem(L"7.5");
    rain->addItem(L"8"); rain->addItem(L"8.5"); rain->addItem(L"9"); rain->addItem(L"9.5");
    rain->addItem(L"10");

    visibility->addItem(L"10.0");visibility->addItem(L"9.5");visibility->addItem(L"9.0");visibility->addItem(L"8.5");
    visibility->addItem(L"8.0");visibility->addItem(L"7.5");visibility->addItem(L"7.0");visibility->addItem(L"6.5");
    visibility->addItem(L"6.0");visibility->addItem(L"5.5");visibility->addItem(L"5.0");visibility->addItem(L"4.5");
    visibility->addItem(L"4.0");visibility->addItem(L"3.5");visibility->addItem(L"3.0");visibility->addItem(L"2.5");
    visibility->addItem(L"2.0");visibility->addItem(L"1.5");visibility->addItem(L"1.0");
    visibility->addItem(L"0.9");visibility->addItem(L"0.8");visibility->addItem(L"0.7");
    visibility->addItem(L"0.6");visibility->addItem(L"0.5");visibility->addItem(L"0.4");
    visibility->addItem(L"0.3"); visibility->addItem(L"0.2"); visibility->addItem(L"0.1"); visibility->addItem(L"0");

    //This is used to track when the edit boxes need updating, when ship or legs have changed. Set to true for initial load
    editBoxesNeedUpdating = true;

    //Fill in initial info into dialog boxes:
    irr::f32 timeFloat = oldScenarioInfo.startTime/SECONDS_IN_HOUR;
    irr::u32 timeHrs = floor(timeFloat);
    irr::u32 timeMins = (timeFloat - timeHrs)*60;
    irr::core::stringw hoursString(timeHrs);
    irr::core::stringw minsString(timeMins);
    if (hoursString.size() == 1) hoursString = irr::core::stringw(L"0") + hoursString;
    if (minsString.size() == 1) minsString = irr::core::stringw(L"0") + minsString;
    startHours->setText(hoursString.c_str());
    startMins->setText(minsString.c_str());

    startYear->setText((irr::core::stringw(oldScenarioInfo.startYear)).c_str());

    descriptionEdit->setText(irr::core::stringw(oldScenarioInfo.description.c_str()).c_str());

    irr::core::stringw monthString(oldScenarioInfo.startMonth);
    if (monthString.size() == 1) monthString = irr::core::stringw(L"0") + monthString;
    startMonth->setText(monthString.c_str());

    irr::core::stringw dayString(oldScenarioInfo.startDay);
    if (dayString.size() == 1) dayString = irr::core::stringw(L"0") + dayString;
    startDay->setText(dayString.c_str());

    //SunRise, SunSet, Weather, Rain
    sunRise->setText((irr::core::stringw(oldScenarioInfo.sunRise)).c_str());
    sunSet->setText((irr::core::stringw(oldScenarioInfo.sunSet)).c_str());
    weather->setSelected(floor(oldScenarioInfo.weather*2));
    rain->setSelected(floor(oldScenarioInfo.rainIntensity*2));

    windDirection->setText(f32To1dp(oldScenarioInfo.windDirection).c_str());
    windSpeed->setText(f32To1dp(oldScenarioInfo.windSpeed).c_str());

    irr::s32 selectedVis;
    if (oldScenarioInfo.visibilityRange<=1) {
        selectedVis = Utilities::round(-10.0*oldScenarioInfo.visibilityRange + 28); //Equation of relation between visibility and items in visibility list where in the 0.1 to 1.0 range, with a spacing of 0.1)
    } else {
        selectedVis = Utilities::round(-2.0*oldScenarioInfo.visibilityRange + 20); //Equation of relation between visibility and items in visibility list where in the 1.0 to 10.0 range, with a spacing of 0.5)
    }
    if(selectedVis >= 0 && selectedVis < visibility->getItemCount()) {
        visibility->setSelected(selectedVis);
    } else if (selectedVis < 0) {
        visibility->setSelected(0);
    } else {
        visibility->setSelected(visibility->getItemCount()-1);
    }

    scenarioName->setText(irr::core::stringw(oldScenarioInfo.scenarioName.c_str()).c_str());

    //These get updated in updateGuiData
    mapCentreX = 0;
    mapCentreZ = 0;

    setActiveTab(0);

    //Add an info box if in multiplayer mode
    if (multiplayer) {
        guienv->addMessageBox(french ? L"Exercice multijoueur" : L"Multiplayer exercise", language->translate("multiplayerinfo").c_str());
    }

}

GUIMain::~GUIMain()
{
    //The start screen is drawn with the skin's font as it was.
    if (originalSkinFont) { guienv->getSkin()->setFont(originalSkinFont); }
}

void GUIMain::setWorldName(const std::string& world)
{
    if (world == worldShown) { return; }
    worldShown = world;
    static_cast<SidePanel*>(sidebar)->setSubtitle((french ? L"Zone : " : L"Area: ") + std::wstring(world.begin(), world.end()));
}

void GUIMain::setActiveTab(int tab)
{
    activeTab = irr::core::clamp(tab, 0, 3);
    for (int i = 0; i < 4; i++) {
        pages[i]->setVisible(i == activeTab);
        tabButtons[i]->setChecked(i == activeTab);
    }
    shipStrip->setVisible(activeTab == 1 || activeTab == 2);
}

irr::core::recti GUIMain::getMapViewport() const
{
    const irr::core::dimension2du s = device->getVideoDriver()->getScreenSize();
    return irr::core::recti(sidebarShown ? sidebarWidth : 0, 0, (irr::s32)s.Width, (irr::s32)s.Height);
}

void GUIMain::placeToolbar()
{
    const irr::s32 x = sidebarShown ? sidebarWidth + 12 : 12;
    toggleUIButton->setRelativePosition(irr::core::rect<irr::s32>(x, 14, x + 40, 14 + 36));
    static_cast<Ui::Button*>(toggleUIButton)->setGlyph(sidebarShown ? Ui::Button::ChevronLeft : Ui::Button::ChevronRight);
}

std::wstring GUIMain::legLabel(const OtherShipData& ship, irr::u32 leg) const
{
    //Number, course, speed and distance of a leg, as in the edit boxes.
    wchar_t text[96];
    const LegData& l = ship.legs.at(leg);
    const irr::f32 hours = (leg + 1 < ship.legs.size()) ? (ship.legs.at(leg + 1).startTime - l.startTime) / SECONDS_IN_HOUR : -1.0f;
    if (fabs(l.speed) < 0.01f) {
        //Stopped: the leg has no end, so no distance.
        swprintf(text, 96, L"%u    %05.1f\u00B0    %ls", leg + 1, l.bearing, french ? L"\u00E0 l'arr\u00EAt" : L"stopped");
    }
    else if (hours >= 0 && hours < 1e6f) {
        swprintf(text, 96, L"%u    %05.1f\u00B0    %.1f %ls    %.2f NM", leg + 1, l.bearing, l.speed, french ? L"nds" : L"kn", hours * l.speed);
    }
    else {
        swprintf(text, 96, L"%u    %05.1f\u00B0    %.1f %ls", leg + 1, l.bearing, l.speed, french ? L"nds" : L"kn");
    }
    return text;
}

void GUIMain::updateEditBoxes()
{
    //Trigger update the edit boxes for course, speed & distance when the selection is changed.
    editBoxesNeedUpdating = true;
}

void GUIMain::updateGuiData(ScenarioData scenarioData, ChartView& chart, const std::vector<PositionData>& buoys, irr::s32 selectedShip, irr::s32 selectedLeg, irr::s32 hoverShip, bool draggingShip, irr::core::position2di mouse)
{
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::gui::IGUIFont* font = mapFont;
    irr::s32 statusBarHeight = (font ? (irr::s32)font->getDimension(L"Ag").Height : 14) + 8;
    if (!scenarioData.worldName.empty()) { setWorldName(scenarioData.worldName); }

    //Show the chart, with a lat/long grid
    chart.draw(driver);
    chart.drawGraticule(driver, font, 0, statusBarHeight);

    std::wstring styleText = (french ? L"Fond : " : L"Background: ") + chart.styleName();
    if (styleText != chartStyleShown) {
        chartStyleButton->setText(styleText.c_str());
        chartStyleShown = styleText;
    }

    //Map centre as displayed (where 'move' and 'add ship' place a ship)
    double centreX, centreZ;
    chart.centreXZ(centreX, centreZ);
    mapCentreX = centreX;
    mapCentreZ = centreZ;

    irr::f32 mapCentreLong = chart.xToLon(centreX);
    irr::f32 mapCentreLat = chart.zToLat(centreZ);

    //Convert lat/long into a readable format
    wchar_t eastWest;
    wchar_t northSouth;
    if (mapCentreLat >= 0) {
        northSouth='N';
    } else {
        northSouth='S';
    }
    if (mapCentreLong >= 0) {
        eastWest='E';
    } else {
        eastWest='W';
    }
    irr::f32 displayLat = fabs(mapCentreLat);
    irr::f32 displayLong = fabs(mapCentreLong);

    irr::f32 latMinutes = (displayLat - (int)displayLat)*60;
    irr::f32 lonMinutes = (displayLong - (int)displayLong)*60;
    irr::u8 latDegrees = (int) displayLat;
    irr::u8 lonDegrees = (int) displayLong;

    //Chart centre, where the + mark is ('new ship' and 'move to centre' use it)
    wchar_t centreText[96];
    swprintf(centreText, 96, L"%ls  %02d\u00B0%06.3f'%lc  %03d\u00B0%06.3f'%lc", french ? L"Centre de la carte (+) :" : L"Chart centre (+):",
        (int)latDegrees, latMinutes, northSouth, (int)lonDegrees, lonMinutes, eastWest);
    irr::core::stringw displayText(centreText);

    //Display
    dataDisplay->setText(displayText.c_str());

    //Note that this section is duplicated in constructor to populate with initial values
    //Show start time & data
    if (oldScenarioInfo.startTime != scenarioData.startTime) {
        irr::f32 timeFloat = scenarioData.startTime/SECONDS_IN_HOUR;
        irr::u32 timeHrs = floor(timeFloat);
        irr::u32 timeMins = (timeFloat - timeHrs)*60;
        irr::core::stringw hoursString(timeHrs);
        irr::core::stringw minsString(timeMins);
        if (hoursString.size() == 1) hoursString = irr::core::stringw(L"0") + hoursString;
        if (minsString.size() == 1) minsString = irr::core::stringw(L"0") + minsString;
        startHours->setText(hoursString.c_str());
        startMins->setText(minsString.c_str());
    }

    if (oldScenarioInfo.startYear != scenarioData.startYear) {
        startYear->setText((irr::core::stringw(scenarioData.startYear)).c_str());
    }

    if (oldScenarioInfo.startMonth != scenarioData.startMonth) {
        irr::core::stringw monthString(scenarioData.startMonth);
        if (monthString.size() == 1) monthString = irr::core::stringw(L"0") + monthString;
        startMonth->setText(monthString.c_str());
    }

    if (oldScenarioInfo.startDay != scenarioData.startDay) {
        irr::core::stringw dayString(scenarioData.startDay);
        if (dayString.size() == 1) dayString = irr::core::stringw(L"0") + dayString;
        startDay->setText(dayString.c_str());
    }

    if (oldScenarioInfo.sunRise != scenarioData.sunRise) {
        sunRise->setText(f32To2dp(scenarioData.sunRise).c_str());
    }
    if (oldScenarioInfo.sunSet != scenarioData.sunSet) {
        sunSet->setText(f32To2dp(scenarioData.sunSet).c_str());
    }
    if (oldScenarioInfo.weather != scenarioData.weather) {
        weather->setSelected(floor(scenarioData.weather*2));
    }
    if (oldScenarioInfo.rainIntensity != scenarioData.rainIntensity) {
        rain->setSelected(floor(scenarioData.rainIntensity*2));
    }
    if (oldScenarioInfo.visibilityRange != scenarioData.visibilityRange) {
        irr::s32 selectedVis;
        if (scenarioData.visibilityRange<=1) {
            selectedVis = Utilities::round(-10.0*scenarioData.visibilityRange + 28.0); //Equation of relation between visibility and items in visibility list where in the 0 to 1.0 range, with a spacing of 0.1)
        } else {
            selectedVis = Utilities::round(-2.0*scenarioData.visibilityRange + 20); //Equation of relation between visibility and items in visibility list where in the 1.0 to 10.0 range, with a spacing of 0.5)
        }
        if(selectedVis >= 0 && selectedVis < visibility->getItemCount()) {
            visibility->setSelected(selectedVis);
        } else if (selectedVis < 0) {
            visibility->setSelected(0);
        } else {
            visibility->setSelected(visibility->getItemCount()-1);
        }
    }
    if (oldScenarioInfo.windDirection != scenarioData.windDirection) {
        windDirection->setText(f32To1dp(scenarioData.windDirection).c_str());
    }
    if (oldScenarioInfo.windSpeed != scenarioData.windSpeed) {
        windSpeed->setText(f32To1dp(scenarioData.windSpeed).c_str());
    }
    if (oldScenarioInfo.scenarioName != scenarioData.scenarioName) {
        scenarioName->setText(irr::core::stringw(scenarioData.scenarioName.c_str()).c_str());
    }

    if (oldScenarioInfo.description != scenarioData.description) {
        descriptionEdit->setText(irr::core::stringw(scenarioData.description.c_str()).c_str());
    }

    //Initially set name colour as default, unless a warning is shown
    scenarioName->enableOverrideColor(false);

    //Check and warn about name validitiy for multiplayer
    if (multiplayer && ! scenarioData.multiplayerName) {
        //Name needs to have _mp at end
        scenarioName->setOverrideColor(irr::video::SColor(255, 255, 165, 0)); //Highlight in orange
        //Show relevant warning
        multiplayerNameWarning->setVisible(true);
        notMultiplayerNameWarning->setVisible(false);
    } else if (!multiplayer && scenarioData.multiplayerName) {
        //Name needs not to have _mp at end
        scenarioName->setOverrideColor(irr::video::SColor(255, 255, 165, 0)); //Highlight in orange
        //Show relevant warning
        notMultiplayerNameWarning->setVisible(true);
        multiplayerNameWarning->setVisible(false);
    } else {
        //Name ok for multiplayer status - hide warnings
        multiplayerNameWarning->setVisible(false);
        notMultiplayerNameWarning->setVisible(false);
    }

    //Check and warn about scenario overwriting
    if (scenarioData.willOverwrite) {
        scenarioName->setOverrideColor(irr::video::SColor(255, 255, 0, 0)); //Highlight in red
        overwriteWarning->setVisible(true); //Show warning
    } else {
        overwriteWarning->setVisible(false); //Hide warning
    }

    //End of duplicated section
    //Store what's been shown
    oldScenarioInfo = scenarioData;

    //Draw centre mark, buoys, ships and their routes, then the status line
    drawInformationOnMap(chart, scenarioData, buoys, selectedShip, selectedLeg, hoverShip);
    drawStatusBar(chart, mouse, draggingShip);

  //KYARA UPDATE -----------------------------------------------------
    if (editBoxesNeedUpdating) {
        if (selectedShip >= 0 && selectedShip < scenarioData.otherShipsData.size()) {

            // --- NEW STRICTLY UNIQUE MMSI LOGIC ---
            irr::u32 currentMmsi = scenarioData.otherShipsData.at(selectedShip).mmsi;
            if (currentMmsi == 0) {
                irr::u32 maxMmsi = 242000100; // Base MMSI
                for (size_t j = 0; j < scenarioData.otherShipsData.size(); j++) {
                    if (scenarioData.otherShipsData[j].mmsi > maxMmsi) {
                        maxMmsi = scenarioData.otherShipsData[j].mmsi;
                    }
                }
                currentMmsi = maxMmsi + 1; // Always grab the next available slot globally
                mmsiEdit->setText(irr::core::stringw(currentMmsi).c_str());
                manuallyTriggerGUIEvent(setMMSI, irr::gui::EGET_BUTTON_CLICKED);
            }
            else {
                mmsiEdit->setText(irr::core::stringw(currentMmsi).c_str());
            }

            isDrifting->setEnabled(true);
            isDrifting->setChecked(scenarioData.otherShipsData.at(selectedShip).drifting);
        }
        else if (selectedShip == -1) {

            // --- AUTO-POPULATE OWNSHIP MMSI ---
            irr::core::stringw currentText = mmsiEdit->getText();
            if (currentText.empty() || currentText == L"-" || currentText == L"0") {
                mmsiEdit->setText(L"242000100"); // Base strictly reserved for Own Ship
                manuallyTriggerGUIEvent(setMMSI, irr::gui::EGET_BUTTON_CLICKED);
            }

            isDrifting->setEnabled(false);
            isDrifting->setChecked(false);
        }

        if (selectedShip >= 0 && selectedShip < scenarioData.otherShipsData.size() && selectedLeg >= 0 && selectedLeg < scenarioData.otherShipsData.at(selectedShip).legs.size()) {
            // FORMATTED TO 1 DECIMAL (E.G. 6.0)
            legCourseEdit->setText(f32To1dp(scenarioData.otherShipsData.at(selectedShip).legs.at(selectedLeg).bearing).c_str());
            legSpeedEdit->setText(f32To1dp(scenarioData.otherShipsData.at(selectedShip).legs.at(selectedLeg).speed).c_str());

            //Distance
            if ((selectedLeg + 1) < scenarioData.otherShipsData.at(selectedShip).legs.size()) {
                irr::f32 legDurationS = scenarioData.otherShipsData.at(selectedShip).legs.at(selectedLeg + 1).startTime - scenarioData.otherShipsData.at(selectedShip).legs.at(selectedLeg).startTime;
                irr::f32 legDurationH = legDurationS / SECONDS_IN_HOUR;
                irr::f32 legDistanceNm = legDurationH * scenarioData.otherShipsData.at(selectedShip).legs.at(selectedLeg).speed;
                //(a stopped leg has no end, and so no distance)
                legDistanceEdit->setText((legDistanceNm >= 0 && legDistanceNm < 1e6f) ? f32To2dp(legDistanceNm).c_str() : L""); // FORMATTED
            }
            else {
                legDistanceEdit->setText(L"");
            }
        }
        else if (selectedShip == -1) {
            //Own ship
            legCourseEdit->setText(f32To1dp(scenarioData.ownShipData.initialBearing).c_str()); // FORMATTED
            legSpeedEdit->setText(f32To1dp(scenarioData.ownShipData.initialSpeed).c_str()); // FORMATTED
            legDistanceEdit->setText(L"---");
        }
        else {
            //Set blank (invalid other ship or leg)
            legCourseEdit->setText(L"");
            legSpeedEdit->setText(L"");
            legDistanceEdit->setText(L"");
        }

        //----------------------------------------------------------------------------------------
        //For visibility of ship selector boxes:
     
        if (selectedShip == -1) {
            otherShipTypeSelector->setVisible(false);
            ownShipTypeSelector->setVisible(true);
            //Find the ship name in the list that matches (if it exists)
            irr::core::stringw ownShipName = irr::core::stringw(scenarioData.ownShipData.ownShipName.c_str());
            for(int i = 0; i < ownShipTypeSelector->getItemCount(); i++) {
                irr::core::stringw thisName(ownShipTypeSelector->getItem(i));
                if (thisName.equals_ignore_case(ownShipName)) {ownShipTypeSelector->setSelected(i);
                //kyara 
                updateShipImageDisplay(getOwnShipTypeSelected());
                
                }
            }
        } else {
            otherShipTypeSelector->setVisible(true);
            ownShipTypeSelector->setVisible(false);
            //Find the ship name in the list that matches (if it exists)
            if (selectedShip >= 0 && selectedShip < scenarioData.otherShipsData.size()) {
                //Find the ship name in the list that matches (if it exists)
                irr::core::stringw otherShipName = irr::core::stringw(scenarioData.otherShipsData.at(selectedShip).shipName.c_str());
                for(int i = 0; i < otherShipTypeSelector->getItemCount(); i++) {
                    irr::core::stringw thisName(otherShipTypeSelector->getItem(i));
                    if (thisName.equals_ignore_case(otherShipName)) {otherShipTypeSelector->setSelected(i);
                    //kyara
                    updateShipImageDisplay(getOtherShipTypeSelected());
                    
                    }
                }
            }
        }

        editBoxesNeedUpdating = false;
    }

    //Update comboboxes for other ships and legs
    updateDropDowns(scenarioData.otherShipsData,selectedShip,scenarioData.startTime);

    //Picture of the model (on the Ships page, when there is room for it)
    const bool pictureRoom = shipImageDisplay->isEnabled();
    shipImageDisplay->setVisible(pictureRoom && hasValidImage);
    shipImageNotFoundText->setVisible(pictureRoom && !hasValidImage);

    //What the route page edits, for the chosen ship
    const std::wstring hint = (selectedShip < 0)
        ? (french ? L"Navire propre : cap et vitesse de d\u00E9part, pris en compte d\u00E8s la saisie. Il n'a pas de route."
                  : L"Own ship: starting course and speed, used as soon as they are typed. It has no route.")
        : (french ? L"Choisissez un segment, changez cap, vitesse ou distance, puis Modifier."
                  : L"Choose a leg, change course, speed or distance, then Change.");
    if (hint != std::wstring(routeHint->getText())) { routeHint->setText(hint.c_str()); }

    guienv->drawAll();

}

namespace {
const irr::video::SColor kOwnShipFill(255, 70, 140, 255);
const irr::video::SColor kOtherShipFill(255, 0, 190, 165);
const irr::video::SColor kOtherShipText(255, 150, 255, 230);
const irr::video::SColor kRouteColour(255, 215, 70, 215);
const irr::video::SColor kSelectedLegColour(255, 255, 160, 0);
const irr::video::SColor kSelectColour(255, 0, 255, 255);
const irr::video::SColor kHoverColour(200, 255, 255, 255);
const irr::video::SColor kShadow(150, 0, 0, 0);
const irr::video::SColor kWhiteColour(255, 255, 255, 255);
const irr::video::SColor kBlackColour(255, 0, 0, 0);
}

void GUIMain::drawLabel(ChartView& chart, const std::wstring& text, irr::core::position2di at, irr::video::SColor colour)
{
    irr::gui::IGUIFont* font = mapFont;
    if (!font) {
        return;
    }
    irr::core::recti clip = chart.getViewport();
    irr::core::dimension2du d = font->getDimension(text.c_str());
    irr::core::recti box(at.X - 2, at.Y, at.X + (irr::s32)d.Width + 2, at.Y + (irr::s32)d.Height);
    box.clipAgainst(clip);
    if (box.isValid()) {
        device->getVideoDriver()->draw2DRectangle(irr::video::SColor(150, 10, 16, 26), box);
    }
    ChartDraw::text(font, text, at, colour, &clip, false);
}

void GUIMain::drawInformationOnMap(ChartView& chart, const ScenarioData& scenarioInfo, const std::vector<PositionData>& buoys, irr::s32 selectedShip, irr::s32 selectedLeg, irr::s32 hoverShip)
{
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    const std::vector<OtherShipData>& otherShips = scenarioInfo.otherShipsData;
    irr::f32 time = scenarioInfo.startTime;

    //Centre mark: where 'move' and 'add ship' place a ship
    irr::core::position2di c = chart.getViewport().getCenter();
    irr::video::SColor markColour = chart.lightBackground() ? irr::video::SColor(220, 40, 50, 60) : irr::video::SColor(220, 230, 230, 230);
    driver->draw2DLine(irr::core::position2di(c.X - 10, c.Y), irr::core::position2di(c.X - 3, c.Y), markColour);
    driver->draw2DLine(irr::core::position2di(c.X + 3, c.Y), irr::core::position2di(c.X + 10, c.Y), markColour);
    driver->draw2DLine(irr::core::position2di(c.X, c.Y - 10), irr::core::position2di(c.X, c.Y - 3), markColour);
    driver->draw2DLine(irr::core::position2di(c.X, c.Y + 3), irr::core::position2di(c.X, c.Y + 10), markColour);

    //Buoys
    for (std::vector<PositionData>::const_iterator it = buoys.begin(); it != buoys.end(); ++it) {
        ChartDraw::marker(driver, chart.toScreenXZ(it->X, it->Z), ChartDraw::Diamond, irr::video::SColor(255, 255, 210, 0), irr::video::SColor(255, 40, 40, 40), 4);
    }

    //Other ships: route, then the ship symbol and its label
    for (irr::u32 i = 0; i < otherShips.size(); i++) {
        const OtherShipData& ship = otherShips.at(i);
        bool selected = (selectedShip == (irr::s32)i);
        irr::core::position2di shipPos = chart.toScreenXZ(ship.initialX, ship.initialZ);

        //Route from the start position along each leg, except the final 'stop' leg. Leg times are from
        //the start of the day of the scenario start; the current leg is the one running at the start.
        if (ship.legs.size() > 1) {
            irr::u32 currentLeg = ship.legs.size() - 1;
            for (irr::u32 l = 0; l + 1 < ship.legs.size(); l++) {
                if (time >= ship.legs.at(l).startTime && time < ship.legs.at(l + 1).startTime) {
                    currentLeg = l;
                }
            }
            std::vector<irr::core::position2di> route;
            std::vector<irr::u32> routeLeg; //leg drawn by the segment ending at route[k + 1]
            route.push_back(shipPos);
            irr::f32 x = ship.initialX;
            irr::f32 z = ship.initialZ;
            for (irr::u32 l = currentLeg; l + 1 < ship.legs.size(); l++) {
                irr::f32 legStart = (l == currentLeg) ? time : ship.legs.at(l).startTime;
                irr::f32 duration = ship.legs.at(l + 1).startTime - legStart;
                if (!(duration < 1e9f)) { break; } //Stopped (zero speed) leg: never ends
                if (!(duration > 0)) { continue; }
                irr::f32 distance = duration * ship.legs.at(l).speed * KTS_TO_MPS;
                x += distance * sin(ship.legs.at(l).bearing * RAD_IN_DEG);
                z += distance * cos(ship.legs.at(l).bearing * RAD_IN_DEG);
                route.push_back(chart.toScreenXZ(x, z));
                routeLeg.push_back(l);
            }
            if (route.size() > 1) {
                irr::video::SColor routeColour = selected ? kRouteColour : irr::video::SColor(170, kRouteColour.getRed(), kRouteColour.getGreen(), kRouteColour.getBlue());
                ChartDraw::polyline(driver, route, routeColour, !selected, selected ? 2 : 1);
                for (irr::u32 k = 0; k < routeLeg.size(); k++) {
                    if (selected && (irr::s32)routeLeg.at(k) == selectedLeg) {
                        std::vector<irr::core::position2di> segment;
                        segment.push_back(route.at(k));
                        segment.push_back(route.at(k + 1));
                        ChartDraw::polyline(driver, segment, kSelectedLegColour, false, 3);
                    }
                    ChartDraw::marker(driver, route.at(k + 1), ChartDraw::Square, routeColour, kBlackColour, selected ? 4 : 3);
                    if (selected) {
                        //Leg number, as in the leg list, half way along the leg
                        irr::core::position2di mid((route.at(k).X + route.at(k + 1).X) / 2, (route.at(k).Y + route.at(k + 1).Y) / 2);
                        drawLabel(chart, std::wstring(irr::core::stringw(routeLeg.at(k) + 1).c_str()), mid + irr::core::position2di(6, -6),
                            (irr::s32)routeLeg.at(k) == selectedLeg ? kSelectedLegColour : kRouteColour);
                    }
                }
            }
        }

        irr::f32 heading = ship.legs.size() > 0 ? ship.legs.at(0).bearing : 0;
        ChartDraw::ship(driver, shipPos, heading, kOtherShipFill, kBlackColour, 12.0f);

        irr::core::stringw label(i + 1);
        label.append(L" ");
        label.append(ship.shipName.c_str());
        drawLabel(chart, std::wstring(label.c_str()), shipPos + irr::core::position2di(21, -8), kOtherShipText);
    }

    //Own ship on top, with a six minute run along its initial course when under way
    irr::core::position2di ownPos = chart.toScreenXZ(scenarioInfo.ownShipData.initialX, scenarioInfo.ownShipData.initialZ);
    if (fabs(scenarioInfo.ownShipData.initialSpeed) > 0.01) {
        irr::f32 run = scenarioInfo.ownShipData.initialSpeed * KTS_TO_MPS * 360.0f;
        irr::f32 bearing = scenarioInfo.ownShipData.initialBearing * RAD_IN_DEG;
        std::vector<irr::core::position2di> vector;
        vector.push_back(ownPos);
        vector.push_back(chart.toScreenXZ(scenarioInfo.ownShipData.initialX + run * sin(bearing), scenarioInfo.ownShipData.initialZ + run * cos(bearing)));
        ChartDraw::polyline(driver, vector, kOwnShipFill, false, 2);
    }
    ChartDraw::ship(driver, ownPos, scenarioInfo.ownShipData.initialBearing, kOwnShipFill, kWhiteColour, 15.0f);
    drawLabel(chart, french ? L"Navire propre" : L"Own ship", ownPos + irr::core::position2di(21, -8), irr::video::SColor(255, 150, 200, 255));

    //Hover and selection rings (selectedShip: -1 own ship, 0.. other ships; hoverShip: 0 own ship, 1.. other ships)
    irr::s32 selectedIndex = selectedShip + 1;
    for (int pass = 0; pass < 2; pass++) {
        irr::s32 ship = (pass == 0) ? hoverShip : selectedIndex;
        if (ship < 0 || ship > (irr::s32)otherShips.size() || (pass == 0 && ship == selectedIndex)) {
            continue;
        }
        irr::core::position2di at = (ship == 0) ? ownPos : chart.toScreenXZ(otherShips.at(ship - 1).initialX, otherShips.at(ship - 1).initialZ);
        ChartDraw::ring(driver, at, 19.0f, kShadow);
        ChartDraw::ring(driver, at, 18.0f, pass == 0 ? kHoverColour : kSelectColour);
    }
}

void GUIMain::drawStatusBar(ChartView& chart, irr::core::position2di mouse, bool draggingShip)
{
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::gui::IGUIFont* font = mapFont;
    if (!font) {
        return;
    }
    const irr::core::recti& vp = chart.getViewport();
    irr::s32 barHeight = (irr::s32)font->getDimension(L"Ag").Height + 8;
    irr::core::recti bar(vp.UpperLeftCorner.X, vp.LowerRightCorner.Y - barHeight, vp.LowerRightCorner.X, vp.LowerRightCorner.Y);
    driver->draw2DRectangle(irr::video::SColor(215, 9, 18, 33), bar);
    driver->draw2DRectangle(irr::video::SColor(90, 110, 160, 220), irr::core::recti(bar.UpperLeftCorner.X, bar.UpperLeftCorner.Y, bar.LowerRightCorner.X, bar.UpperLeftCorner.Y + 1));

    //Cursor position, then the mouse and key controls
    std::wstring text;
    if (vp.isPointInside(mouse)) {
        IncidentPoint p = chart.toLatLong(mouse);
        wchar_t buf[64];
        double lat = fabs(p.lat);
        double lon = fabs(p.lon);
        swprintf(buf, 64, L"%02d\u00B0%06.3f'%lc  %03d\u00B0%06.3f'%lc", (int)lat, (lat - (int)lat) * 60.0, p.lat >= 0 ? L'N' : L'S',
            (int)lon, (lon - (int)lon) * 60.0, p.lon >= 0 ? L'E' : L'W');
        text = buf;
        text += L"    ";
    }
    if (draggingShip) {
        text += french ? L"Rel\u00E2chez pour poser le navire" : L"Release to place the ship";
    } else {
        text += french ? L"clic sur un navire : le choisir, glisser : le d\u00E9placer    glisser la carte : la d\u00E9placer    molette : zoom    Origine : recentrer    fl\u00E8ches gauche/droite : cap du navire s\u00E9lectionn\u00E9"
                       : L"click a ship: choose it, drag: move it    drag the chart: pan    wheel: zoom    Home: centre    left/right arrows: heading of the chosen ship";
    }
    ChartDraw::text(font, text, irr::core::position2di(bar.UpperLeftCorner.X + 10, bar.UpperLeftCorner.Y + 4), irr::video::SColor(255, 210, 220, 232), &bar, false);

    ChartDraw::scaleBar(driver, font, chart, barHeight);
}

void GUIMain::selectShip(irr::s32 shipIndex)
{
    if (shipIndex < 0 || shipIndex >= (irr::s32)shipSelector->getItemCount()) {
        return;
    }
    if (activeTab != 1 && activeTab != 2) { setActiveTab(1); } //Ships tab, to show the chosen ship's details
    if (shipSelector->getSelected() != shipIndex) {
        shipSelector->setSelected(shipIndex);
        manuallyTriggerGUIEvent((irr::gui::IGUIElement*)shipSelector, irr::gui::EGET_COMBO_BOX_CHANGED);
    }
}

void GUIMain::updateDropDowns(const std::vector<OtherShipData>& otherShips, irr::s32 selectedShip, irr::f32 time) {

//Update drop down menus for ships and legs

    //Update text in ship selector list. If a new item, make sure it's selected
    irr::s32 shipSelectorSelection = shipSelector->getSelected();
    bool changedShipSelectorLength = (shipSelector->getItemCount() != otherShips.size() + 1);
    bool initialiseList = (shipSelector->getItemCount() == 0); //If there were no items in list, then we're populating it for the first time (we'll use this to select the first item)
    shipSelector->clear();
    shipSelector->addItem(french ? L"Navire propre" : L"Own ship"); //add own ship (at index 0)
    for(irr::u32 i = 0; i<otherShips.size(); i++) { //Add other ships (at index 1,2,...)
        irr::core::stringw otherShipLabel(irr::core::stringw(i+1));
        otherShipLabel.append(L" ");
        otherShipLabel.append(otherShips.at(i).shipName.c_str());
        shipSelector->addItem(otherShipLabel.c_str());
    }
    //Set selection
    if (changedShipSelectorLength) {
        //Select the first item if new, or the last one if it's just been added to the existing list
        if (initialiseList) {
            shipSelector->setSelected(0);
        } else {
            shipSelector->setSelected(shipSelector->getItemCount()-1); //Select the newly added item (I think that the 'trigger gui event' should make sure that the model selection follows suit
        }
        manuallyTriggerGUIEvent((irr::gui::IGUIElement*)shipSelector, irr::gui::EGET_COMBO_BOX_CHANGED); //Trigger event here so any changes caused by the update are found
    } else {
        //Re-select previously selected item
        shipSelector->setSelected(shipSelectorSelection);
    }

    //Find number of legs for selected ship if known
    irr::u32 selectedShipNoLegs = 0;
    if (selectedShip>=0) {
        if (otherShips.size() > selectedShip) { //SelectedShip is valid
            selectedShipNoLegs = otherShips.at(selectedShip).legs.size();
        }
    }
    //Update number of legs displayed, if required
    if(selectedShipNoLegs>0) {selectedShipNoLegs--;} //Note that we display legs-1, as the final 'stop' leg shouldn't be changed by the user
    if(legSelector->getItemCount() != selectedShipNoLegs) {
        legSelector->clear();
        for(irr::u32 i = 0; i<selectedShipNoLegs; i++) {
            legSelector->addItem(irr::core::stringw(i+1).c_str());
        }
        manuallyTriggerGUIEvent((irr::gui::IGUIElement*)legSelector, irr::gui::EGET_LISTBOX_CHANGED ); //Trigger event here so any changes caused by the update are found

    } else {
        //don't clear and update, but show each leg's values
        if (legSelector->getItemCount() > 0) {

            //Get legs for selected ship
            if (selectedShip>=0 && otherShips.size() > selectedShip) { //SelectedShip is valid
                const std::vector<LegData>& selectedShipLegs = otherShips.at(selectedShip).legs;

                //Number, course, speed and distance of each leg.
                for (irr::u32 i=0; i<legSelector->getItemCount() && i<selectedShipLegs.size(); i++) {
                    const std::wstring label = legLabel(otherShips.at(selectedShip), i);
                    if (label != std::wstring(legSelector->getListItem(i))) {
                        legSelector->setItem(i,label.c_str(),-1);
                    }
                }

            } //Selected ships valid
        } //At least one leg in selector
    } //Update descriptive text on legs, if they don't need updating entirely

}

bool GUIMain::manuallyTriggerGUIEvent(irr::gui::IGUIElement* caller, irr::gui::EGUI_EVENT_TYPE eType) {

    irr::SEvent triggerUpdateEvent;
    triggerUpdateEvent.EventType = irr::EET_GUI_EVENT;
    triggerUpdateEvent.GUIEvent.Caller = caller;
    triggerUpdateEvent.GUIEvent.Element = 0;
    triggerUpdateEvent.GUIEvent.EventType = eType;
    return device->postEventFromUser(triggerUpdateEvent);
}

irr::f32 GUIMain::getEditBoxCourse() const {
    //irr::f32 course = _wtof(legCourseEdit->getText()); //TODO: Check portability
    wchar_t* endPtr;
    irr::f32 course = wcstof(legCourseEdit->getText(), &endPtr);
    return course;
}

irr::f32 GUIMain::getEditBoxSpeed() const {
    //irr::f32 speed = _wtof(legSpeedEdit->getText()); //TODO: Check portability
    wchar_t* endPtr;
    irr::f32 speed = wcstof(legSpeedEdit->getText(), &endPtr); //TODO: Check portability
    return speed;
}

irr::f32 GUIMain::getEditBoxDistance() const {
    //irr::f32 distance = _wtof(legDistanceEdit->getText()); //TODO: Check portability
    wchar_t* endPtr;
    irr::f32 distance = wcstof(legDistanceEdit->getText(), &endPtr); //TODO: Check portability
    return distance;
}

irr::u32 GUIMain::getEditBoxMMSI() const {
    wchar_t* endPtr;
    irr::u32 mmsi = wcstol(mmsiEdit->getText(),&endPtr,10); //TODO: Check portability
    return mmsi;
}

int GUIMain::getSelectedShip() const {
    return shipSelector->getSelected();
}

int GUIMain::getSelectedLeg() const {
    //Note that this returns the leg, starting at 0 (Different from controller implementation, which starts at 1 (not 0))
    return legSelector->getSelected();
}


std::string GUIMain::getOwnShipTypeSelected() const {
    //Todo: Instead of this, should probably use the strings directly from 'std::vector<std::string> ownShipTypes'
    if (ownShipTypeSelector->getSelected()<0) {return "";} //If nothing selected
    std::wstring wideName(ownShipTypeSelector->getItem(ownShipTypeSelector->getSelected()));
    std::string nameString(wideName.begin(), wideName.end());
    return nameString;
}

std::string GUIMain::getOtherShipTypeSelected() const {
    //Todo: Instead of this, should probably use the strings directly from 'std::vector<std::string> otherShipTypes'

    if (otherShipTypeSelector->getSelected()<0) {return "";} //If nothing selected
    std::wstring wideName(otherShipTypeSelector->getItem(otherShipTypeSelector->getSelected()));
    std::string nameString(wideName.begin(),wideName.end());
    return nameString;
}


irr::core::vector2df GUIMain::getScreenCentrePosition() const {
    return irr::core::vector2df(mapCentreX, mapCentreZ);
}

/*
irr::gui::IGUIEditBox* startHours;
    irr::gui::IGUIEditBox* startMins;
    irr::gui::IGUIEditBox* startDay;
    irr::gui::IGUIEditBox* startMonth;
    irr::gui::IGUIEditBox* startYear;
    irr::gui::IGUIEditBox* sunRise;
    irr::gui::IGUIEditBox* sunSet;
    irr::gui::IGUIComboBox* weather;
    irr::gui::IGUIComboBox* rain;
*/

irr::f32 GUIMain::getStartTime() const {
    wchar_t* endPtr;
    irr::f32 hours = floor(wcstof(startHours->getText(),&endPtr));
    wchar_t* endPtr2; //Is this needed? Is endPtr changed in the call above
    irr::f32 mins = floor(wcstof(startMins->getText(),&endPtr2));

    return ((hours + mins/60.0) * SECONDS_IN_HOUR);
}

irr::u32 GUIMain::getStartDay() const {
    wchar_t* endPtr;
    return wcstof(startDay->getText(),&endPtr);
}

irr::u32 GUIMain::getStartMonth() const {
    wchar_t* endPtr;
    return wcstof(startMonth->getText(),&endPtr);
}

irr::u32 GUIMain::getStartYear() const {
    wchar_t* endPtr;
    return wcstof(startYear->getText(),&endPtr);
}

irr::f32 GUIMain::getSunRise() const {
    wchar_t* endPtr;
    return wcstof(sunRise->getText(),&endPtr);
}

irr::f32 GUIMain::getSunSet() const {
    wchar_t* endPtr;
    return wcstof(sunSet->getText(),&endPtr);
}

irr::f32 GUIMain::getWeather() const {

    return ((irr::f32)weather->getSelected())/2.0; //Entries for integer and half values, so Nth entry is for N/2
}

irr::f32 GUIMain::getRain() const {
    return ((irr::f32)rain->getSelected())/2.0;
}

irr::f32 GUIMain::getVisibility() const {
    //Get value from string in drop down.
    std::wstring wStringVal = std::wstring(visibility->getText());
    std::string sStringVal(wStringVal.begin(), wStringVal.end());
    irr::f32 value = Utilities::lexical_cast<irr::f32>(sStringVal);
    return value;
}

irr::f32 GUIMain::getWindDirection() const {
    wchar_t* endPtr;
    return wcstof(windDirection->getText(),&endPtr);
}
    
irr::f32 GUIMain::getWindSpeed() const {
    wchar_t* endPtr;
    return wcstof(windSpeed->getText(),&endPtr);
}

std::string GUIMain::getScenarioName() const {

    //Convert from wide to narrow string: Todo: Think about having this all wide.
    std::wstring wideName(scenarioName->getText());
    std::string scenarioNameString(wideName.begin(),wideName.end());

    //Strip any invalid characters: /\*:"|?<>
    replace(scenarioNameString.begin(), scenarioNameString.end(),'/',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'\\',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'*',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),':',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'"',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'|',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'?',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'<',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'>',' ');

    scenarioNameString = Utilities::trim(scenarioNameString);

    return scenarioNameString;
}

std::string GUIMain::getDescription() const {
    //Convert from wide to narrow string: Todo: Think about having this all wide.
    std::wstring wideDescription(descriptionEdit->getText());
    std::string descriptionString(wideDescription.begin(),wideDescription.end());
    return descriptionString;
}

std::wstring GUIMain::f32To3dp(irr::f32 value) const
{
    //Convert a floating point value to a wstring, with 3dp
    char tempStr[100];
    snprintf(tempStr,100,"%.3f",value);
    return std::wstring(tempStr, tempStr+strlen(tempStr));
}

std::wstring GUIMain::f32To4dp(irr::f32 value) const
{
    //Convert a floating point value to a wstring, with 3dp
    char tempStr[100];
    snprintf(tempStr,100,"%.4f",value);
    return std::wstring(tempStr, tempStr+strlen(tempStr));

}

std::wstring GUIMain::f32To2dp(irr::f32 value) const
{
    char tempStr[100];
    snprintf(tempStr, 100, "%.2f", value);
    return std::wstring(tempStr, tempStr + strlen(tempStr));
}

std::wstring GUIMain::f32To1dp(irr::f32 value) const
{
    char tempStr[100];
    snprintf(tempStr, 100, "%.1f", value);
    return std::wstring(tempStr, tempStr + strlen(tempStr));
}


//kyara 
void GUIMain::updateShipImageDisplay(std::string shipName) {
    if (shipName.empty()) {
        hasValidImage = false;
        return;
    }

    // Path to the image folder you created
    std::string imagePath = "boat_pictures/" + shipName + ".png";

    // Attempt to load the texture
    irr::video::ITexture* texture = device->getVideoDriver()->getTexture(imagePath.c_str());

    if (texture) {
        shipImageDisplay->setImage(texture);
        hasValidImage = true;
    }
    else {
        hasValidImage = false;
    }

}
bool GUIMain::wantsToReturnToMenu() const {
    return returnToMenuFlag;
}

void GUIMain::setReturnToMenu() {
    returnToMenuFlag = true;
}


//hide/show the side panel (the chart then uses the whole window)
void GUIMain::hideUI() {
    sidebarShown = false;
    sidebar->setVisible(false);
    placeToolbar();
}

void GUIMain::toggleUI() {
    sidebarShown = !sidebarShown;
    sidebar->setVisible(sidebarShown);
    placeToolbar();
}
