//Start screens of the two scenario editors (scenario editor, fire / SAR editor), in the style of the
//simulator's exercise screen: a header, cards, a chart of the map selected and a footer, drawn behind
//the editors' own lists and buttons, which keep their ids and event receivers.
//
//Layout: maps on the left, the chart and the map's details in the middle, the exercises and their
//details on the right. The editor places its controls in the areas given here.

#ifndef __EDITORSTARTSCREEN_HPP_INCLUDED__
#define __EDITORSTARTSCREEN_HPP_INCLUDED__

#include "irrlicht.h"
#include "UiTheme.hpp"
#include "chartView/ChartView.hpp"
#include <functional>
#include <map>
#include <string>

class EditorStartScreen : public irr::gui::IGUIElement
{
public:
    //scenarioWorld: the map of exercise n of the exercise list (for the chart), "" if unknown.
    typedef std::function<std::string(irr::s32)> ScenarioWorldFn;

    //Covers the parent; create it before the editor's controls so that it is drawn behind them.
    //accent: the editor's colour (blue for the scenario editor, orange for fire / SAR).
    EditorStartScreen(irr::IrrlichtDevice* device, irr::gui::IGUIElement* parent, const std::wstring& title, const std::wstring& subtitle,
        irr::video::SColor accent, bool french)
        : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, device->getGUIEnvironment(), parent ? parent : device->getGUIEnvironment()->getRootGUIElement(),
            -1, irr::core::rect<irr::s32>(0, 0, 10, 10)),
        device(device), title(title), subtitle(subtitle), accent(accent), french(french), worldList(0), scenarioList(0),
        lastWorld(-2), lastScenario(-2), showScenario(false), chart(0), pending(false), s(1)
    {
        const irr::core::dimension2du screen = device->getVideoDriver()->getScreenSize();
        const irr::core::rect<irr::s32> full = Parent ? Parent->getAbsolutePosition() : irr::core::rect<irr::s32>(0, 0, (irr::s32)screen.Width, (irr::s32)screen.Height);
        setRelativePosition(irr::core::rect<irr::s32>(0, 0, full.getWidth(), full.getHeight()));
        W = (irr::f32)full.getWidth();
        H = (irr::f32)full.getHeight();
        s = irr::core::clamp(std::min(W / 1600.0f, H / 950.0f), 0.7f, 2.0f);
        titleFont = font(26);
        headingFont = font(15);
        textFont = font(16);
        smallFont = font(13);
        setTabStop(false);
    }

    virtual ~EditorStartScreen()
    {
        for (std::map<std::string, ChartView*>::iterator it = charts.begin(); it != charts.end(); ++it) { delete it->second; }
        if (worldList) { worldList->drop(); }
        if (scenarioList) { scenarioList->drop(); }
    }

    //The bitmap font of the given size (bc5.ini font), cleaned; for the editors' lists too.
    irr::gui::IGUIFont* font(int size) const
    {
        size = irr::core::clamp((int)(size * s + 0.5f), 12, 36);
        irr::gui::IGUIFont* f = Environment->getFont(("media/fonts/noto-sans/noto-sans-" + std::to_string(size) + ".xml").c_str());
        Ui::cleanFontAtlas(f);
        return f;
    }
    irr::f32 scale() const { return s; }

    //The lists whose selection the chart follows: the map selected, or the map of the exercise
    //selected, whichever was chosen last.
    void follow(irr::gui::IGUIListBox* worlds, irr::gui::IGUIListBox* scenarios, ScenarioWorldFn scenarioWorld)
    {
        //Held, so that the lists can be removed before this screen without it reading freed ones.
        if (worlds) { worlds->grab(); }
        if (scenarios) { scenarios->grab(); }
        if (worldList) { worldList->drop(); }
        if (scenarioList) { scenarioList->drop(); }
        worldList = worlds;
        scenarioList = scenarios;
        worldOfScenario = scenarioWorld;
    }

    void setFooter(const std::wstring& text) { footer = text; }

    //--- Areas (absolute pixels) -------------------------------------------------------------------
    irr::f32 headerHeight() const { return std::floor(84 * s); }
    irr::f32 footerTop() const { return H - std::floor(44 * s); }
    irr::f32 margin() const { return std::floor(26 * s); }
    irr::f32 gap() const { return std::floor(18 * s); }
    irr::f32 top() const { return headerHeight() + gap(); }
    irr::f32 bottom() const { return footerTop() - std::floor(10 * s); }

    irr::core::rect<irr::f32> leftCard() const { return irr::core::rect<irr::f32>(margin(), top(), margin() + std::floor(W * 0.24f), bottom()); }
    irr::core::rect<irr::f32> rightCard() const { return irr::core::rect<irr::f32>(W - margin() - std::floor(W * 0.29f), top(), W - margin(), top() + (bottom() - top()) * 0.46f); }
    irr::core::rect<irr::f32> previewCard() const
    {
        return irr::core::rect<irr::f32>(leftCard().LowerRightCorner.X + gap(), top(), rightCard().UpperLeftCorner.X - gap(), top() + (bottom() - top()) * 0.64f);
    }
    irr::core::rect<irr::f32> mapInfoCard() const
    {
        return irr::core::rect<irr::f32>(previewCard().UpperLeftCorner.X, previewCard().LowerRightCorner.Y + gap(), previewCard().LowerRightCorner.X, bottom());
    }
    //Under the exercise list: a row of buttons (actionRow), the details card, a row of buttons (bottomRow).
    irr::f32 buttonHeight() const { return std::floor(40 * s); }
    irr::core::rect<irr::f32> actionRow() const
    {
        const irr::core::rect<irr::f32> r = rightCard();
        return irr::core::rect<irr::f32>(r.UpperLeftCorner.X, r.LowerRightCorner.Y + gap() * 0.7f, r.LowerRightCorner.X, r.LowerRightCorner.Y + gap() * 0.7f + buttonHeight());
    }
    irr::core::rect<irr::f32> bottomRow() const
    {
        const irr::core::rect<irr::f32> r = rightCard();
        return irr::core::rect<irr::f32>(r.UpperLeftCorner.X, bottom() - buttonHeight(), r.LowerRightCorner.X, bottom());
    }
    irr::core::rect<irr::f32> detailsCard() const
    {
        const irr::core::rect<irr::f32> r = rightCard();
        return irr::core::rect<irr::f32>(r.UpperLeftCorner.X, actionRow().LowerRightCorner.Y + gap() * 0.7f, r.LowerRightCorner.X, bottomRow().UpperLeftCorner.Y - gap() * 0.7f);
    }
    //Inside a card, under its title (and above `reserve` pixels kept at the bottom).
    irr::core::rect<irr::s32> content(const irr::core::rect<irr::f32>& card, irr::f32 reserve = 0) const
    {
        const irr::f32 p = std::floor(14 * s);
        return irr::core::rect<irr::s32>((irr::s32)(card.UpperLeftCorner.X + p * 0.6f), (irr::s32)(card.UpperLeftCorner.Y + 40 * s),
            (irr::s32)(card.LowerRightCorner.X - p * 0.6f), (irr::s32)(card.LowerRightCorner.Y - p * 0.6f - reserve));
    }
    //Two buttons side by side in a row.
    irr::core::rect<irr::s32> half(const irr::core::rect<irr::f32>& row, int which) const
    {
        const irr::f32 w = (row.getWidth() - gap() * 0.6f) * 0.5f;
        const irr::f32 x = row.UpperLeftCorner.X + which * (w + gap() * 0.6f);
        return irr::core::rect<irr::s32>((irr::s32)x, (irr::s32)row.UpperLeftCorner.Y, (irr::s32)(x + w), (irr::s32)row.LowerRightCorner.Y);
    }
    irr::core::rect<irr::s32> toInt(const irr::core::rect<irr::f32>& r) const { return Ui::toI(r); }

    //Card titles
    void setCardTitles(const std::wstring& maps, const std::wstring& mapInfo, const std::wstring& exercises, const std::wstring& details)
    {
        mapsTitle = maps;
        mapInfoTitle = mapInfo;
        exercisesTitle = exercises;
        detailsTitle = details;
    }

    virtual void draw()
    {
        if (!IsVisible) { return; }
        irr::video::IVideoDriver* driver = Environment->getVideoDriver();
        followSelection();

        irr::gui::PanelBatch b;
        b.begin(driver);
        //Page, header and footer bands
        b.rectV(irr::core::rect<irr::f32>(0, 0, W, H), irr::video::SColor(255, 12, 24, 42), Ui::backgroundDeep);
        b.rectV(irr::core::rect<irr::f32>(0, 0, W, headerHeight()), irr::video::SColor(255, 17, 33, 58), irr::video::SColor(255, 13, 27, 48));
        b.rect(irr::core::rect<irr::f32>(0, headerHeight() - 1, W, headerHeight()), Ui::rule);
        b.rectH(irr::core::rect<irr::f32>(0, headerHeight() - 2 * s, W * 0.35f, headerHeight()), accent, irr::video::SColor(0, accent.getRed(), accent.getGreen(), accent.getBlue()));
        b.rectV(irr::core::rect<irr::f32>(0, footerTop(), W, H), irr::video::SColor(255, 13, 26, 46), irr::video::SColor(255, 10, 20, 36));
        b.rect(irr::core::rect<irr::f32>(0, footerTop(), W, footerTop() + 1), Ui::rule);
        //Emblem: a ship's wheel in the editor's colour
        const irr::core::vector2df mark(margin() + 24 * s, headerHeight() * 0.5f);
        b.disc(mark, 24 * s, accent, irr::video::SColor(255, accent.getRed() * 6 / 10, accent.getGreen() * 6 / 10, accent.getBlue() * 6 / 10));
        drawWheel(b, mark, 14 * s, irr::video::SColor(255, 255, 255, 255));
        //Cards
        Ui::card(b, leftCard(), 14);
        Ui::card(b, previewCard(), 14);
        Ui::card(b, mapInfoCard(), 14);
        Ui::card(b, rightCard(), 14);
        Ui::card(b, detailsCard(), 14);
        b.flush();

        //Header text
        Ui::drawText(titleFont, title, irr::core::rect<irr::f32>(mark.X + 38 * s, headerHeight() * 0.5f - 26 * s, W - margin(), headerHeight() * 0.5f + 4 * s), Ui::text);
        Ui::drawText(smallFont, subtitle, irr::core::rect<irr::f32>(mark.X + 38 * s, headerHeight() * 0.5f + 4 * s, W - margin(), headerHeight() * 0.5f + 26 * s), Ui::textDim);
        //Card titles
        cardTitle(leftCard(), mapsTitle);
        cardTitle(mapInfoCard(), mapInfoTitle);
        cardTitle(rightCard(), exercisesTitle);
        cardTitle(detailsCard(), detailsTitle);
        drawPreview(driver);
        if (!footer.empty()) {
            Ui::drawText(smallFont, footer, irr::core::rect<irr::f32>(margin(), footerTop(), W * 0.7f, H), Ui::textFaint);
        }
        IGUIElement::draw();
    }

private:
    void cardTitle(const irr::core::rect<irr::f32>& card, const std::wstring& text)
    {
        if (text.empty()) { return; }
        Ui::drawText(headingFont, text, irr::core::rect<irr::f32>(card.UpperLeftCorner.X + 16 * s, card.UpperLeftCorner.Y + 8 * s,
            card.LowerRightCorner.X - 16 * s, card.UpperLeftCorner.Y + 34 * s), titleColour());
    }

    //The editor's colour, lighter: card titles.
    irr::video::SColor titleColour() const
    {
        return irr::video::SColor(255, (accent.getRed() * 2 + 255) / 3, (accent.getGreen() * 2 + 255) / 3, (accent.getBlue() * 2 + 255) / 3);
    }

    void drawWheel(irr::gui::PanelBatch& b, irr::core::vector2df c, irr::f32 r, irr::video::SColor col)
    {
        const irr::f32 lw = std::max(1.6f, r * 0.14f);
        b.sector(c, r * 0.5f, r * 0.5f + lw, 0, 360, col, col);
        b.disc(c, r * 0.2f, col, col);
        for (int i = 0; i < 8; i++) {
            const irr::f32 a = 22.5f + 45.0f * i;
            b.line(irr::gui::panelPolar(c, r * 0.18f, a), irr::gui::panelPolar(c, r * 0.86f, a), lw, col);
            b.disc(irr::gui::panelPolar(c, r * 0.92f, a), lw * 0.85f, col, col);
        }
    }

    //Which map to show: the one selected last, in either list.
    void followSelection()
    {
        const irr::s32 w = worldList ? worldList->getSelected() : -1;
        const irr::s32 e = scenarioList ? scenarioList->getSelected() : -1;
        if (w != lastWorld && lastWorld != -2) { showScenario = false; }
        if (e != lastScenario && lastScenario != -2) { showScenario = true; }
        if (lastWorld == -2) { showScenario = false; }
        lastWorld = w;
        lastScenario = e;
        std::string world, caption;
        if (showScenario && e >= 0 && worldOfScenario) {
            world = worldOfScenario(e);
            caption = std::string(irr::core::stringc(scenarioList->getListItem(e)).c_str());
        }
        else if (w >= 0) {
            world = std::string(irr::core::stringc(worldList->getListItem(w)).c_str());
        }
        if (world != shownWorld) {
            shownWorld = world;
            chart = 0;
            pending = !world.empty();
        }
        shownCaption = caption;
    }

    void drawPreview(irr::video::IVideoDriver* driver)
    {
        const irr::core::rect<irr::f32> card = previewCard();
        std::wstring heading = french ? L"CARTE" : L"CHART";
        if (!shownWorld.empty()) { heading += L"  \u00B7  " + std::wstring(irr::core::stringw(shownWorld.c_str()).c_str()); }
        if (!shownCaption.empty()) { heading += (french ? L"  \u00B7  exercice " : L"  \u00B7  exercise ") + std::wstring(irr::core::stringw(shownCaption.c_str()).c_str()); }
        const irr::core::rect<irr::s32> area = content(card);
        const irr::core::rect<irr::s32> titleClip((irr::s32)card.UpperLeftCorner.X, (irr::s32)card.UpperLeftCorner.Y, (irr::s32)card.LowerRightCorner.X - (irr::s32)(16 * s), area.UpperLeftCorner.Y);
        Ui::drawText(headingFont, heading, irr::core::rect<irr::f32>(card.UpperLeftCorner.X + 16 * s, card.UpperLeftCorner.Y + 8 * s,
            card.LowerRightCorner.X - 16 * s, card.UpperLeftCorner.Y + 34 * s), titleColour(), Ui::Left, &titleClip);
        //Map: loaded the frame after it is asked for, so that "loading" shows meanwhile
        if (pending) {
            std::map<std::string, ChartView*>::iterator it = charts.find(shownWorld);
            if (it != charts.end()) {
                chart = it->second;
                pending = false;
            }
            else if (loadNext) {
                ChartView* c = new ChartView();
                c->setPreviewStyle(ChartView::Style_Day);
                std::string error;
                if (!c->load(device, shownWorld, error)) {
                    delete c;
                    c = 0;
                }
                charts[shownWorld] = c;
                chart = c;
                pending = false;
                loadNext = false;
            }
            else {
                loadNext = true;
            }
        }
        const irr::core::rect<irr::f32> inner = Ui::toF(area);
        if (chart) {
            chart->setViewport(area);
            chart->fitWorld();
            driver->draw2DRectangle(irr::video::SColor(255, 8, 16, 30), area);
            chart->draw(driver);
            chart->drawGraticule(driver, smallFont, 0, 0);
        }
        else {
            driver->draw2DRectangle(irr::video::SColor(255, 8, 16, 30), area);
            const std::wstring message = pending ? (french ? L"Chargement de la carte\u2026" : L"Loading the chart\u2026")
                : (shownWorld.empty() ? (french ? L"Choisissez une carte" : L"Choose a chart") : (french ? L"Carte indisponible" : L"Chart unavailable"));
            Ui::drawText(textFont, message, inner, Ui::textDim, Ui::Centre);
        }
    }

    irr::IrrlichtDevice* device;
    std::wstring title, subtitle, footer;
    std::wstring mapsTitle, mapInfoTitle, exercisesTitle, detailsTitle;
    irr::video::SColor accent;
    bool french;
    irr::gui::IGUIListBox* worldList;
    irr::gui::IGUIListBox* scenarioList;
    ScenarioWorldFn worldOfScenario;
    irr::s32 lastWorld, lastScenario;
    bool showScenario;
    std::string shownWorld, shownCaption;
    std::map<std::string, ChartView*> charts;
    ChartView* chart;
    bool pending;
    bool loadNext = false;
    irr::gui::IGUIFont* titleFont;
    irr::gui::IGUIFont* headingFont;
    irr::gui::IGUIFont* textFont;
    irr::gui::IGUIFont* smallFont;
    irr::f32 W, H, s;
};

#endif
