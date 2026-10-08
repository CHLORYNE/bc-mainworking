/*   NAUTITECH - Simulateur de Navigation
     The instructor's controls window (CONTROLES), drawn like the weather window.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "ControlsPanel.hpp"
#include "BridgeSkin.hpp"
#include "GUIPanelDraw.hpp"
#include "launcher/HudText.hpp"

#include <cmath>
#include <cwchar>
#include <algorithm>

namespace
{
    irr::video::SColor mix(irr::video::SColor a, irr::video::SColor b, irr::f32 t)
    {
        return b.getInterpolated(a, irr::core::clamp(t, 0.0f, 1.0f));
    }
    irr::video::SColor withAlpha(irr::video::SColor c, irr::u32 a)
    {
        c.setAlpha(a);
        return c;
    }
    irr::core::rect<irr::s32> toI(const irr::core::rect<irr::f32>& r)
    {
        return irr::core::rect<irr::s32>((irr::s32)floorf(r.UpperLeftCorner.X + 0.5f), (irr::s32)floorf(r.UpperLeftCorner.Y + 0.5f),
            (irr::s32)floorf(r.LowerRightCorner.X + 0.5f), (irr::s32)floorf(r.LowerRightCorner.Y + 0.5f));
    }
    void fill(irr::video::IVideoDriver* d, const irr::core::rect<irr::f32>& r, irr::video::SColor c, const irr::core::rect<irr::s32>* clip)
    {
        d->draw2DRectangle(c, toI(r), clip);
    }
    void outline(irr::video::IVideoDriver* d, const irr::core::rect<irr::s32>& r, irr::video::SColor c, const irr::core::rect<irr::s32>* clip)
    {
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.UpperLeftCorner.Y + 1), clip);
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.LowerRightCorner.Y - 1, r.LowerRightCorner.X, r.LowerRightCorner.Y), clip);
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.UpperLeftCorner.X + 1, r.LowerRightCorner.Y), clip);
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(r.LowerRightCorner.X - 1, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y), clip);
    }
    std::wstring textOf(irr::gui::IGUIElement* e)
    {
        return (e && e->getText()) ? std::wstring(e->getText()) : std::wstring();
    }
}

//=================================================================================================

ControlsPanel::ControlsPanel(irr::gui::IGUIEnvironment* environment, irr::gui::IGUIElement* parent, const irr::core::rect<irr::s32>& area)
    : IGUIElement(irr::gui::EGUIET_ELEMENT, environment, parent, -1, area),
    titleIcon(0), tab(0), hoveredRow(-1), selectedRow(-1), draggedRow(-1), hoveredKey(-1), pressedRow(-1), pressedKey(-1),
    hoveredTab(-1), hoveredClose(false), k(1.0f)
{
    setVisible(false);
}

ControlsPanel::~ControlsPanel()
{
    freeFonts();
}

//-------------------------------------------------------------------------------------------------
//Building
//-------------------------------------------------------------------------------------------------

int ControlsPanel::addTab(const std::wstring& name, const std::wstring& help, StatusFn status)
{
    Tab t;
    t.name = name;
    t.help = help;
    t.status = status;
    tabs.push_back(t);
    return (int)tabs.size() - 1;
}

void ControlsPanel::addSection(int t, const std::wstring& label)
{
    if (t < 0 || t >= (int)tabs.size()) { return; }
    Row r;
    r.kind = ROW_SECTION;
    r.label = label;
    tabs[t].rows.push_back(r);
}

void ControlsPanel::addSlider(int t, const std::wstring& label, const std::wstring& help, irr::gui::IGUIScrollBar* bar,
    irr::gui::IGUIStaticText* value)
{
    if (t < 0 || t >= (int)tabs.size() || !bar) { return; }
    Row r;
    r.kind = ROW_SLIDER;
    r.label = label;
    r.help = help;
    r.bar = bar;
    r.text = value;
    tabs[t].rows.push_back(r);
}

void ControlsPanel::addToggle(int t, const std::wstring& label, const std::wstring& help, irr::gui::IGUICheckBox* box)
{
    if (t < 0 || t >= (int)tabs.size() || !box) { return; }
    Row r;
    r.kind = ROW_TOGGLE;
    r.label = label;
    r.help = help;
    r.box = box;
    tabs[t].rows.push_back(r);
}

void ControlsPanel::addKeys(int t, const std::wstring& label, const std::wstring& help, const std::vector<irr::gui::IGUIButton*>& keys,
    const std::vector<std::wstring>& names, std::function<int()> selected)
{
    if (t < 0 || t >= (int)tabs.size() || keys.empty()) { return; }
    Row r;
    r.kind = ROW_KEYS;
    r.label = label;
    r.help = help;
    r.keys = keys;
    r.names = names;
    r.names.resize(keys.size());
    r.selected = selected;
    tabs[t].rows.push_back(r);
}

void ControlsPanel::addChoice(int t, const std::wstring& label, const std::wstring& help, irr::gui::IGUIComboBox* combo)
{
    if (t < 0 || t >= (int)tabs.size() || !combo) { return; }
    Row r;
    r.kind = ROW_CHOICE;
    r.label = label;
    r.help = help;
    r.combo = combo;
    tabs[t].rows.push_back(r);
}

void ControlsPanel::addText(int t, irr::gui::IGUIStaticText* text, int lines, std::function<bool()> alert)
{
    if (t < 0 || t >= (int)tabs.size() || !text) { return; }
    Row r;
    r.kind = ROW_TEXT;
    r.text = text;
    r.lines = std::max(1, lines);
    r.alert = alert;
    tabs[t].rows.push_back(r);
}

//-------------------------------------------------------------------------------------------------
//Layout (the same proportions as the weather window)
//-------------------------------------------------------------------------------------------------

void ControlsPanel::loadFonts()
{
    freeFonts();
    irr::video::IVideoDriver* driver = Environment->getVideoDriver();
    irr::gui::IGUIFont* fallback = Environment->getSkin() ? Environment->getSkin()->getFont() : 0;
    const std::string folder = "media/fonts/barlow-condensed/";
    fonts.title = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 34 * k, fallback);
    fonts.tab = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 20 * k, fallback);
    fonts.label = new HudFont(driver, folder + "BarlowCondensed-Regular.ttf", 21 * k, fallback);
    fonts.value = new HudFont(driver, folder + "BarlowCondensed-Medium.ttf", 20 * k, fallback);
    fonts.section = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 16 * k, fallback);
    fonts.infoTitle = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 30 * k, fallback);
    fonts.infoText = new HudFont(driver, folder + "BarlowCondensed-Regular.ttf", 21 * k, fallback);
    fonts.caption = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 18 * k, fallback);
}

void ControlsPanel::freeFonts()
{
    HudFont** all[] = { &fonts.title, &fonts.tab, &fonts.label, &fonts.value, &fonts.section, &fonts.infoTitle, &fonts.infoText, &fonts.caption };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        delete *all[i];
        *all[i] = 0;
    }
}

void ControlsPanel::updateAbsolutePosition()
{
    IGUIElement::updateAbsolutePosition();
    if (AbsoluteRect != laidOutRect) { layout(); }
}

void ControlsPanel::layout()
{
    laidOutRect = AbsoluteRect;
    const irr::core::rect<irr::f32> r((irr::f32)AbsoluteRect.UpperLeftCorner.X, (irr::f32)AbsoluteRect.UpperLeftCorner.Y,
        (irr::f32)AbsoluteRect.LowerRightCorner.X, (irr::f32)AbsoluteRect.LowerRightCorner.Y);
    const irr::f32 W = r.getWidth(), H = r.getHeight();
    if (W < 10 || H < 10) { return; }
    const irr::f32 newK = irr::core::clamp(std::min(H / 720.0f, W / 1200.0f), 0.6f, 2.0f);
    if (fabsf(newK - k) > 0.01f || !fonts.title) {
        k = newK;
        loadFonts();
    }
    const irr::f32 pad = 22 * k;
    header = irr::core::rect<irr::f32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.UpperLeftCorner.Y + 62 * k);
    tabsBar = irr::core::rect<irr::f32>(r.UpperLeftCorner.X + pad, header.LowerRightCorner.Y, r.LowerRightCorner.X - pad, header.LowerRightCorner.Y + 40 * k);
    const irr::f32 bodyTop = tabsBar.LowerRightCorner.Y + 18 * k;
    const irr::f32 bodyBottom = r.LowerRightCorner.Y - pad;
    const irr::f32 listW = (W - 3 * pad) * 0.6f;
    list = irr::core::rect<irr::f32>(r.UpperLeftCorner.X + pad, bodyTop, r.UpperLeftCorner.X + pad + listW, bodyBottom);
    info = irr::core::rect<irr::f32>(list.LowerRightCorner.X + pad, bodyTop, r.LowerRightCorner.X - pad, bodyBottom);
    clampScroll();
}

irr::core::rect<irr::f32> ControlsPanel::tabRect(int t) const
{
    const int n = std::max(1, (int)tabs.size());
    const irr::f32 gap = 6 * k;
    const irr::f32 w = std::min(168 * k, (tabsBar.getWidth() - (n - 1) * gap) / n);
    const irr::f32 total = n * w + (n - 1) * gap;
    const irr::f32 x0 = (tabsBar.UpperLeftCorner.X + tabsBar.LowerRightCorner.X) * 0.5f - total * 0.5f + t * (w + gap);
    return irr::core::rect<irr::f32>(x0, tabsBar.UpperLeftCorner.Y, x0 + w, tabsBar.LowerRightCorner.Y);
}

irr::core::rect<irr::f32> ControlsPanel::closeRect() const
{
    const irr::f32 s = 34 * k;
    const irr::f32 x1 = header.LowerRightCorner.X - 16 * k, y0 = header.UpperLeftCorner.Y + (header.getHeight() - s) * 0.5f;
    return irr::core::rect<irr::f32>(x1 - s, y0, x1, y0 + s);
}

irr::f32 ControlsPanel::rowHeight(const Row& row) const
{
    switch (row.kind) {
    case ROW_SECTION: return 30 * k;
    case ROW_TEXT: return row.lines * (fonts.infoText ? fonts.infoText->lineHeight() + 2 * k : 24 * k) + 12 * k;
    default: return 40 * k;
    }
}

irr::core::rect<irr::f32> ControlsPanel::rowRect(int index) const
{
    if (tab < 0 || tab >= (int)tabs.size()) { return irr::core::rect<irr::f32>(); }
    const std::vector<Row>& rs = tabs[tab].rows;
    irr::f32 y = list.UpperLeftCorner.Y - tabs[tab].scroll;
    for (int i = 0; i < index && i < (int)rs.size(); i++) {
        y += rowHeight(rs[i]) + 4 * k;
    }
    const irr::f32 h = index < (int)rs.size() ? rowHeight(rs[index]) : 40 * k;
    return irr::core::rect<irr::f32>(list.UpperLeftCorner.X, y, list.LowerRightCorner.X - 14 * k, y + h);
}

irr::f32 ControlsPanel::split(const irr::core::rect<irr::f32>& row) const
{
    return row.UpperLeftCorner.X + row.getWidth() * 0.46f;
}

irr::core::rect<irr::f32> ControlsPanel::valueCell(const Row& row, const irr::core::rect<irr::f32>& r) const
{
    if (row.kind == ROW_KEYS && row.label.empty()) { return r; }  //keys across the whole row
    return irr::core::rect<irr::f32>(split(r) + 4 * k, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y);
}

irr::core::rect<irr::f32> ControlsPanel::sliderTrack(const irr::core::rect<irr::f32>& cell) const
{
    //Value on the left of the cell, the track after it
    const irr::f32 x0 = cell.UpperLeftCorner.X + std::min(118 * k, cell.getWidth() * 0.42f), x1 = cell.LowerRightCorner.X - 18 * k;
    const irr::f32 cy = cell.getCenter().Y;
    return irr::core::rect<irr::f32>(x0, cy - 3 * k, x1, cy + 3 * k);
}

irr::core::rect<irr::f32> ControlsPanel::keyRect(const Row& row, const irr::core::rect<irr::f32>& cell, int index) const
{
    const int n = std::max(1, (int)row.keys.size());
    const irr::f32 gap = 4 * k;
    const irr::f32 w = (cell.getWidth() - (n - 1) * gap) / n;
    const irr::f32 x = cell.UpperLeftCorner.X + index * (w + gap);
    return irr::core::rect<irr::f32>(x, cell.UpperLeftCorner.Y, x + w, cell.LowerRightCorner.Y);
}

irr::f32 ControlsPanel::contentHeight() const
{
    if (tab < 0 || tab >= (int)tabs.size() || tabs[tab].rows.empty()) { return 0; }
    const irr::core::rect<irr::f32> last = rowRect((int)tabs[tab].rows.size() - 1);
    return last.LowerRightCorner.Y + tabs[tab].scroll - list.UpperLeftCorner.Y;
}

void ControlsPanel::clampScroll()
{
    if (tab < 0 || tab >= (int)tabs.size()) { return; }
    const irr::f32 maxScroll = std::max(0.0f, contentHeight() - list.getHeight());
    tabs[tab].scroll = irr::core::clamp(tabs[tab].scroll, 0.0f, maxScroll);
}

int ControlsPanel::rowAt(const irr::core::vector2df& p) const
{
    if (tab < 0 || tab >= (int)tabs.size() || !list.isPointInside(p)) { return -1; }
    const std::vector<Row>& rs = tabs[tab].rows;
    for (int i = 0; i < (int)rs.size(); i++) {
        if (rs[i].kind == ROW_SECTION) { continue; }
        if (rowRect(i).isPointInside(p)) { return i; }
    }
    return -1;
}

int ControlsPanel::keyAt(int row, const irr::core::vector2df& p) const
{
    if (row < 0 || tab < 0 || tab >= (int)tabs.size() || row >= (int)tabs[tab].rows.size()) { return -1; }
    const Row& r = tabs[tab].rows[row];
    if (r.kind != ROW_KEYS) { return -1; }
    const irr::core::rect<irr::f32> cell = valueCell(r, rowRect(row));
    for (int i = 0; i < (int)r.keys.size(); i++) {
        if (keyRect(r, cell, i).isPointInside(p)) { return i; }
    }
    return -1;
}

void ControlsPanel::send(irr::gui::IGUIElement* caller, irr::gui::EGUI_EVENT_TYPE type)
{
    if (!caller) { return; }
    irr::SEvent e;
    e.EventType = irr::EET_GUI_EVENT;
    e.GUIEvent.Caller = caller;
    e.GUIEvent.Element = 0;
    e.GUIEvent.EventType = type;
    //The environment hands GUI events to the application's receiver (MyEventReceiver)
    Environment->getRootGUIElement()->OnEvent(e);
}

void ControlsPanel::dragSlider(int rowIndex, irr::s32 mouseX)
{
    if (tab < 0 || tab >= (int)tabs.size() || rowIndex < 0 || rowIndex >= (int)tabs[tab].rows.size()) { return; }
    const Row& row = tabs[tab].rows[rowIndex];
    if (row.kind != ROW_SLIDER || !row.bar) { return; }
    const irr::core::rect<irr::f32> track = sliderTrack(valueCell(row, rowRect(rowIndex)));
    irr::f32 t = ((irr::f32)mouseX - track.UpperLeftCorner.X) / std::max(1.0f, track.getWidth());
    t = irr::core::clamp(t, 0.0f, 1.0f);
    const irr::s32 lo = row.bar->getMin(), hi = row.bar->getMax();
    const irr::s32 v = lo + (irr::s32)floorf(t * (hi - lo) + 0.5f);
    if (v != row.bar->getPos()) {
        row.bar->setPos(v);
        send(row.bar, irr::gui::EGET_SCROLL_BAR_CHANGED);
    }
}

//-------------------------------------------------------------------------------------------------
//Mouse
//-------------------------------------------------------------------------------------------------

bool ControlsPanel::OnEvent(const irr::SEvent& event)
{
    if (!IsVisible || !IsEnabled) { return IGUIElement::OnEvent(event); }
    if (event.EventType == irr::EET_GUI_EVENT) {
        if (event.GUIEvent.EventType == irr::gui::EGET_ELEMENT_FOCUS_LOST && event.GUIEvent.Caller == this) { draggedRow = -1; }
        return IGUIElement::OnEvent(event);
    }
    if (event.EventType != irr::EET_MOUSE_INPUT_EVENT) { return IGUIElement::OnEvent(event); }

    const irr::SEvent::SMouseInput& m = event.MouseInput;
    const irr::core::position2di p(m.X, m.Y);
    const irr::core::vector2df pf((irr::f32)m.X, (irr::f32)m.Y);
    switch (m.Event) {
    case irr::EMIE_MOUSE_MOVED:
        if (draggedRow >= 0) { dragSlider(draggedRow, m.X); return true; }
        hoveredClose = closeRect().isPointInside(pf);
        hoveredTab = -1;
        for (int t = 0; t < (int)tabs.size(); t++) { if (tabRect(t).isPointInside(pf)) { hoveredTab = t; } }
        hoveredRow = rowAt(pf);
        hoveredKey = keyAt(hoveredRow, pf);
        return AbsoluteRect.isPointInside(p);
    case irr::EMIE_LMOUSE_PRESSED_DOWN: {
        if (!AbsoluteRect.isPointInside(p)) { return false; }
        if (closeRect().isPointInside(pf)) {
            setVisible(false);
            Environment->removeFocus(this);
            return true;
        }
        for (int t = 0; t < (int)tabs.size(); t++) {
            if (tabRect(t).isPointInside(pf)) {
                tab = t;
                selectedRow = hoveredRow = hoveredKey = -1;
                clampScroll();
                return true;
            }
        }
        const int row = rowAt(pf);
        if (row < 0) { return true; }
        selectedRow = row;
        const Row& r = tabs[tab].rows[row];
        const irr::core::rect<irr::f32> cell = valueCell(r, rowRect(row));
        //Switches and choices also answer a click on their label; sliders and keys only in their cell
        if (r.kind == ROW_TOGGLE) {
            if (r.box->isEnabled()) {
                r.box->setChecked(!r.box->isChecked());
                send(r.box, irr::gui::EGET_CHECKBOX_CHANGED);
            }
        }
        else if (r.kind == ROW_SLIDER && cell.isPointInside(pf)) {
            draggedRow = row;
            Environment->setFocus(this); //the moves come here while dragging, even off the row
            dragSlider(row, m.X);
        }
        else if (r.kind == ROW_KEYS) {
            const int key = keyAt(row, pf);
            if (key >= 0 && r.keys[key]->isEnabled()) {
                pressedRow = row;
                pressedKey = key;
                send(r.keys[key], irr::gui::EGET_BUTTON_CLICKED);
            }
        }
        else if (r.kind == ROW_CHOICE && r.combo->getItemCount() > 0) {
            const int n = (int)r.combo->getItemCount();
            const int current = irr::core::clamp(r.combo->getSelected(), 0, n - 1);
            const bool forward = !cell.isPointInside(pf) || pf.X >= cell.getCenter().X;
            r.combo->setSelected((current + (forward ? 1 : n - 1)) % n);
            send(r.combo, irr::gui::EGET_COMBO_BOX_CHANGED);
        }
        return true;
    }
    case irr::EMIE_LMOUSE_LEFT_UP:
        pressedRow = pressedKey = -1;
        if (draggedRow >= 0) {
            draggedRow = -1;
            Environment->removeFocus(this); //keys go back to the ship
            return true;
        }
        return AbsoluteRect.isPointInside(p);
    case irr::EMIE_MOUSE_WHEEL:
        if (!AbsoluteRect.isPointInside(p)) { return false; }
        if (list.isPointInside(pf) && tab >= 0 && tab < (int)tabs.size()) {
            tabs[tab].scroll -= m.Wheel * 60.0f * k;
            clampScroll();
        }
        return true;
    default:
        return AbsoluteRect.isPointInside(p);
    }
}

//-------------------------------------------------------------------------------------------------
//Drawing
//-------------------------------------------------------------------------------------------------

void ControlsPanel::draw()
{
    if (!IsVisible) { return; }
    if (AbsoluteRect != laidOutRect) { layout(); }
    if (!fonts.title) { return; }
    if (tab >= (int)tabs.size()) { tab = 0; }
    irr::video::IVideoDriver* driver = Environment->getVideoDriver();
    const bridge::Palette& pal = bridge::palette();
    const irr::core::rect<irr::s32>& clip = AbsoluteClippingRect;

    //Window: translucent, with a shadow and a hairline edge
    irr::core::rect<irr::s32> shadow = AbsoluteRect;
    shadow += irr::core::position2di(0, (irr::s32)(6 * k));
    bridge::fillRound(driver, shadow, (irr::s32)(10 * k), irr::video::SColor(70, 0, 0, 0), irr::video::SColor(70, 0, 0, 0), 0);
    bridge::fillRound(driver, AbsoluteRect, (irr::s32)(10 * k), withAlpha(pal.panelTop, 236), withAlpha(pal.panelBottom, 236), 0);
    outline(driver, AbsoluteRect, pal.edge, 0);

    //Title, the name of the open tab on the right, the close box
    const irr::f32 tx = header.UpperLeftCorner.X + 24 * k;
    bridge::drawIcon(driver, (bridge::Icon)titleIcon, tx, header.getCenter().Y - 15 * k, 30 * k, pal.accent);
    fonts.title->drawIn(title, irr::core::rect<irr::f32>(tx + 42 * k, header.UpperLeftCorner.Y, tx + 400 * k, header.LowerRightCorner.Y),
        pal.text, HudFont::Left, 1.5f * k);
    const irr::core::rect<irr::f32> cr = closeRect();
    if (tab >= 0 && tab < (int)tabs.size()) {
        fonts.caption->drawIn(tabs[tab].name, irr::core::rect<irr::f32>(tx + 300 * k, header.UpperLeftCorner.Y, cr.UpperLeftCorner.X - 20 * k, header.LowerRightCorner.Y),
            pal.textDim, HudFont::Right, 0.5f * k);
    }
    if (hoveredClose) { bridge::fillRound(driver, toI(cr), (irr::s32)(5 * k), pal.danger, pal.danger, 0); }
    {
        const irr::video::SColor xc = hoveredClose ? pal.dangerText : pal.textDim;
        const irr::f32 in = cr.getWidth() * 0.32f;
        irr::gui::PanelBatch b;
        b.begin(driver);
        b.line(irr::core::vector2df(cr.UpperLeftCorner.X + in, cr.UpperLeftCorner.Y + in), irr::core::vector2df(cr.LowerRightCorner.X - in, cr.LowerRightCorner.Y - in), 2.2f * k, xc);
        b.line(irr::core::vector2df(cr.LowerRightCorner.X - in, cr.UpperLeftCorner.Y + in), irr::core::vector2df(cr.UpperLeftCorner.X + in, cr.LowerRightCorner.Y - in), 2.2f * k, xc);
        b.flush();
    }
    fill(driver, irr::core::rect<irr::f32>(AbsoluteRect.UpperLeftCorner.X + 1, header.LowerRightCorner.Y - 1, AbsoluteRect.LowerRightCorner.X - 1, header.LowerRightCorner.Y), withAlpha(pal.edge, 160), &clip);

    //Tabs
    for (int t = 0; t < (int)tabs.size(); t++) {
        const irr::core::rect<irr::f32> r = tabRect(t);
        const irr::core::rect<irr::f32> rr(r.UpperLeftCorner.X, r.UpperLeftCorner.Y + 8 * k, r.LowerRightCorner.X, r.LowerRightCorner.Y);
        irr::video::SColor face = (t == tab) ? pal.accent : (t == hoveredTab ? pal.keyHover : pal.raised);
        bridge::fillRound(driver, toI(rr), (irr::s32)(3 * k), face, face, 0);
        if (t != tab) { outline(driver, toI(rr), withAlpha(pal.edge, 200), 0); }
        fonts.tab->drawIn(hudUpper(tabs[t].name), rr, t == tab ? pal.accentText : pal.text, HudFont::Centre, 1.2f * k);
    }
    fill(driver, irr::core::rect<irr::f32>(list.UpperLeftCorner.X, tabsBar.LowerRightCorner.Y + 8 * k, info.LowerRightCorner.X, tabsBar.LowerRightCorner.Y + 9 * k),
        withAlpha(pal.edge, 140), &clip);

    //The list (clipped to its own area, it scrolls)
    irr::core::rect<irr::s32> listClip = toI(list);
    listClip.clipAgainst(clip);
    drawRows(driver, listClip);

    //Scroll bar, when there is more than fits
    const irr::f32 total = contentHeight();
    if (total > list.getHeight() + 1 && tab < (int)tabs.size()) {
        const irr::f32 x = list.LowerRightCorner.X - 4 * k;
        fill(driver, irr::core::rect<irr::f32>(x - 1.5f * k, list.UpperLeftCorner.Y, x + 1.5f * k, list.LowerRightCorner.Y), withAlpha(pal.edge, 160), &clip);
        const irr::f32 thumbH = std::max(30 * k, list.getHeight() * list.getHeight() / total);
        const irr::f32 thumbY = list.UpperLeftCorner.Y + (list.getHeight() - thumbH) * tabs[tab].scroll / std::max(1.0f, total - list.getHeight());
        bridge::fillRound(driver, toI(irr::core::rect<irr::f32>(x - 2.5f * k, thumbY, x + 2.5f * k, thumbY + thumbH)), (irr::s32)(2 * k), pal.textDim, pal.textDim, &clip);
    }

    drawInfo(driver);
}

void ControlsPanel::drawRows(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip)
{
    if (tab < 0 || tab >= (int)tabs.size()) { return; }
    const bridge::Palette& pal = bridge::palette();
    const std::vector<Row>& rs = tabs[tab].rows;
    for (int i = 0; i < (int)rs.size(); i++) {
        const Row& row = rs[i];
        const irr::core::rect<irr::f32> r = rowRect(i);
        if (r.LowerRightCorner.Y < list.UpperLeftCorner.Y || r.UpperLeftCorner.Y > list.LowerRightCorner.Y) { continue; }
        if (row.kind == ROW_SECTION) {
            const irr::f32 ly = r.LowerRightCorner.Y - 6 * k;
            fonts.section->drawIn(hudUpper(row.label), irr::core::rect<irr::f32>(r.UpperLeftCorner.X + 2 * k, r.UpperLeftCorner.Y, r.LowerRightCorner.X, ly - 2 * k),
                pal.textDim, HudFont::Left, 1.6f * k, &clip);
            fill(driver, irr::core::rect<irr::f32>(r.UpperLeftCorner.X, ly, r.LowerRightCorner.X, ly + 1), withAlpha(pal.edge, 170), &clip);
            continue;
        }
        if (row.kind == ROW_TEXT) {
            //Read-only: what the simulation says, on a plain field
            bridge::fillRound(driver, toI(r), (irr::s32)(3 * k), withAlpha(pal.field, 200), withAlpha(pal.field, 200), &clip);
            const bool alert = row.alert && row.alert();
            const irr::f32 px = r.UpperLeftCorner.X + 14 * k;
            if (alert) { fill(driver, irr::core::rect<irr::f32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.UpperLeftCorner.X + 4 * k, r.LowerRightCorner.Y), pal.error, &clip); }
            const std::vector<std::wstring> lines = fonts.infoText->wrap(textOf(row.text), r.getWidth() - 28 * k);
            const irr::f32 lh = fonts.infoText->lineHeight() + 2 * k;
            irr::f32 ly = r.UpperLeftCorner.Y + 6 * k;
            for (size_t l = 0; l < lines.size() && (int)l < row.lines; l++, ly += lh) {
                fonts.infoText->drawIn(lines[l], irr::core::rect<irr::f32>(px, ly, r.LowerRightCorner.X - 12 * k, ly + lh),
                    alert ? pal.error : pal.text, HudFont::Left, 0, &clip);
            }
            continue;
        }

        const bool active = (i == selectedRow) || (i == draggedRow);
        const bool hover = (i == hoveredRow);
        const bool enabled = !(row.kind == ROW_TOGGLE && !row.box->isEnabled());
        const irr::core::rect<irr::f32> cell = valueCell(row, r);

        //Label cell: highlighted when it is the row in use
        if (!(row.kind == ROW_KEYS && row.label.empty())) {
            const irr::core::rect<irr::f32> labelCell(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, split(r), r.LowerRightCorner.Y);
            irr::video::SColor face = active ? pal.accent : (hover ? mix(pal.raised, pal.accent, 0.25f) : pal.raised);
            bridge::fillRound(driver, toI(labelCell), (irr::s32)(3 * k), withAlpha(face, 235), withAlpha(face, 235), &clip);
            fonts.label->drawIn(row.label, irr::core::rect<irr::f32>(labelCell.UpperLeftCorner.X + 16 * k, labelCell.UpperLeftCorner.Y, labelCell.LowerRightCorner.X - 8 * k, labelCell.LowerRightCorner.Y),
                active ? pal.accentText : (enabled ? pal.text : pal.textFaint), HudFont::Left, 0.3f * k, &clip);
        }

        if (row.kind == ROW_KEYS) {
            const int chosen = row.selected ? row.selected() : -1;
            for (int j = 0; j < (int)row.keys.size(); j++) {
                const irr::core::rect<irr::f32> kr = keyRect(row, cell, j);
                const bool on = (j == chosen);
                const bool down = (i == pressedRow && j == pressedKey);
                const bool over = (i == hoveredRow && j == hoveredKey);
                irr::video::SColor face = on ? pal.accent : (down ? mix(pal.key, pal.accent, 0.35f) : (over ? pal.keyHover : withAlpha(pal.field, 240)));
                bridge::fillRound(driver, toI(kr), (irr::s32)(3 * k), face, face, &clip);
                if (!on) { outline(driver, toI(kr), over ? withAlpha(pal.accent, 200) : withAlpha(pal.edge, 200), &clip); }
                fonts.value->drawIn(row.names[j], irr::core::rect<irr::f32>(kr.UpperLeftCorner.X + 6 * k, kr.UpperLeftCorner.Y, kr.LowerRightCorner.X - 6 * k, kr.LowerRightCorner.Y),
                    on ? pal.accentText : pal.text, HudFont::Centre, 0, &clip);
            }
            continue;
        }

        //Value cell
        bridge::fillRound(driver, toI(cell), (irr::s32)(3 * k), withAlpha(pal.field, 240), withAlpha(pal.field, 240), &clip);
        if ((hover || active) && enabled) { outline(driver, toI(cell), withAlpha(pal.accent, 180), &clip); }

        if (row.kind == ROW_SLIDER) {
            const irr::core::rect<irr::f32> track = sliderTrack(cell);
            fonts.value->drawIn(textOf(row.text), irr::core::rect<irr::f32>(cell.UpperLeftCorner.X + 10 * k, cell.UpperLeftCorner.Y, track.UpperLeftCorner.X - 12 * k, cell.LowerRightCorner.Y),
                pal.text, HudFont::Left, 0, &clip);
            const irr::s32 lo = row.bar->getMin(), hi = row.bar->getMax();
            const irr::f32 t = irr::core::clamp((irr::f32)(row.bar->getPos() - lo) / (irr::f32)std::max(1, hi - lo), 0.0f, 1.0f);
            const irr::f32 kx = track.UpperLeftCorner.X + t * track.getWidth();
            fill(driver, track, withAlpha(pal.edge, 220), &clip);
            fill(driver, irr::core::rect<irr::f32>(track.UpperLeftCorner.X, track.UpperLeftCorner.Y, kx, track.LowerRightCorner.Y), pal.accent, &clip);
            //Diamond knob
            const irr::f32 ks = (i == draggedRow ? 9.0f : 7.5f) * k;
            const irr::f32 cy = track.getCenter().Y;
            irr::gui::PanelBatch b;
            b.begin(driver);
            b.quad(irr::core::vector2df(kx, cy - ks - 1.5f * k), irr::core::vector2df(kx + ks + 1.5f * k, cy), irr::core::vector2df(kx, cy + ks + 1.5f * k), irr::core::vector2df(kx - ks - 1.5f * k, cy), pal.field);
            b.quad(irr::core::vector2df(kx, cy - ks), irr::core::vector2df(kx + ks, cy), irr::core::vector2df(kx, cy + ks), irr::core::vector2df(kx - ks, cy), active ? pal.accent : pal.text);
            b.flush();
        }
        else if (row.kind == ROW_TOGGLE) {
            //A switch on the right, its state in words on the left
            const bool on = row.box->isChecked();
            const irr::f32 sh = 22 * k, sw = 44 * k;
            const irr::f32 sx1 = cell.LowerRightCorner.X - 14 * k, sy0 = cell.getCenter().Y - sh * 0.5f;
            const irr::core::rect<irr::f32> sw_r(sx1 - sw, sy0, sx1, sy0 + sh);
            const irr::video::SColor trackCol = !enabled ? withAlpha(pal.edge, 120) : (on ? pal.accent : withAlpha(pal.edge, 230));
            bridge::fillRound(driver, toI(sw_r), (irr::s32)(sh * 0.5f), trackCol, trackCol, &clip);
            const irr::f32 kr = sh * 0.5f - 3 * k;
            const irr::core::vector2df kc(on ? sw_r.LowerRightCorner.X - sh * 0.5f : sw_r.UpperLeftCorner.X + sh * 0.5f, sw_r.getCenter().Y);
            irr::gui::PanelBatch b;
            b.begin(driver);
            b.disc(kc, kr, enabled ? pal.field : withAlpha(pal.field, 160), enabled ? pal.field : withAlpha(pal.field, 160));
            b.flush();
            const wchar_t* word = !enabled ? L"absent" : (on ? L"oui" : L"non");
            fonts.value->drawIn(hudUpper(word), irr::core::rect<irr::f32>(cell.UpperLeftCorner.X + 10 * k, cell.UpperLeftCorner.Y, sw_r.UpperLeftCorner.X - 8 * k, cell.LowerRightCorner.Y),
                !enabled ? pal.textFaint : (on ? pal.accent : pal.textDim), HudFont::Left, 1.0f * k, &clip);
        }
        else if (row.kind == ROW_CHOICE) {
            const int n = (int)row.combo->getItemCount();
            const int current = n > 0 ? irr::core::clamp(row.combo->getSelected(), 0, n - 1) : -1;
            const bool dashes = n > 1 && n <= 10;
            const irr::core::rect<irr::f32> textBox(cell.UpperLeftCorner.X + 28 * k, cell.UpperLeftCorner.Y - (dashes ? 3 * k : 0), cell.LowerRightCorner.X - 28 * k, cell.LowerRightCorner.Y - (dashes ? 3 * k : 0));
            if (current >= 0) { fonts.value->drawIn(row.combo->getItem(current), textBox, pal.text, HudFont::Centre, 0, &clip); }
            //Arrows at both ends, one dash per option underneath (when they fit)
            const irr::f32 cy = cell.getCenter().Y, a = 5 * k;
            irr::gui::PanelBatch b;
            b.begin(driver);
            const irr::f32 lx = cell.UpperLeftCorner.X + 14 * k, rx = cell.LowerRightCorner.X - 14 * k;
            b.tri(irr::core::vector2df(lx - a * 0.6f, cy), irr::core::vector2df(lx + a * 0.6f, cy - a), irr::core::vector2df(lx + a * 0.6f, cy + a), pal.textDim);
            b.tri(irr::core::vector2df(rx + a * 0.6f, cy), irr::core::vector2df(rx - a * 0.6f, cy + a), irr::core::vector2df(rx - a * 0.6f, cy - a), pal.textDim);
            if (dashes) {
                const irr::f32 dw = 12 * k, dg = 3 * k;
                const irr::f32 dx0 = cell.getCenter().X - (n * dw + (n - 1) * dg) * 0.5f;
                const irr::f32 dy = cell.LowerRightCorner.Y - 7 * k;
                for (int j = 0; j < n; j++) {
                    b.rect(irr::core::rect<irr::f32>(dx0 + j * (dw + dg), dy, dx0 + j * (dw + dg) + dw, dy + 2.5f * k), j == current ? pal.accent : withAlpha(pal.textDim, 150));
                }
            }
            b.flush();
        }
    }
}

void ControlsPanel::drawInfo(irr::video::IVideoDriver* driver)
{
    if (tab < 0 || tab >= (int)tabs.size()) { return; }
    const bridge::Palette& pal = bridge::palette();
    const irr::core::rect<irr::s32>& clip = AbsoluteClippingRect;
    const Tab& t = tabs[tab];

    //What the card is about: the row (or key) under the mouse, else the one last used, else the tab
    std::wstring cardTitle = t.name, help = t.help;
    const int r = hoveredRow >= 0 ? hoveredRow : selectedRow;
    if (r >= 0 && r < (int)t.rows.size() && t.rows[r].kind != ROW_TEXT) {
        const Row& row = t.rows[r];
        const int key = (r == hoveredRow) ? hoveredKey : -1;
        if (key >= 0) {
            cardTitle = row.label.empty() ? row.names[key] : row.label + L" : " + row.names[key];
            help.clear();
            if (row.keys[key]->getToolTipText().size() > 0) { help = row.keys[key]->getToolTipText().c_str(); }
            if (help.empty()) { help = row.help; }
        }
        else {
            cardTitle = row.label.empty() ? t.name : row.label;
            help = row.help;
            if (help.empty()) {
                irr::gui::IGUIElement* w = row.bar ? (irr::gui::IGUIElement*)row.bar : row.box ? (irr::gui::IGUIElement*)row.box : (irr::gui::IGUIElement*)row.combo;
                if (w) { help = w->getToolTipText().c_str(); }
            }
        }
    }

    //The status readouts of the tab, at the bottom of the column
    std::vector<Readout> status;
    if (t.status) { status = t.status(); }
    const irr::f32 lh = fonts.infoText->lineHeight() + 2 * k;
    std::vector<std::vector<std::wstring> > statusLines;
    irr::f32 statusH = 0;
    for (size_t i = 0; i < status.size(); i++) {
        statusLines.push_back(fonts.infoText->wrap(status[i].text, info.getWidth() - 28 * k));
        if (statusLines.back().empty()) { statusLines.back().push_back(L"-"); }
        statusH += 20 * k + statusLines.back().size() * lh + 10 * k;
    }
    if (!status.empty()) { statusH += 34 * k; }
    statusH = std::min(statusH, info.getHeight() * 0.62f);

    //Card: title, a rule, the explanation
    const irr::core::rect<irr::f32> card(info.UpperLeftCorner.X, info.UpperLeftCorner.Y, info.LowerRightCorner.X,
        info.LowerRightCorner.Y - (status.empty() ? 0 : statusH + 14 * k));
    bridge::fillRound(driver, toI(card), (irr::s32)(4 * k), withAlpha(pal.raised, 220), withAlpha(pal.panelBottom, 120), &clip);
    const irr::f32 px = card.UpperLeftCorner.X + 14 * k;
    fonts.infoTitle->drawIn(hudUpper(cardTitle), irr::core::rect<irr::f32>(px, card.UpperLeftCorner.Y + 6 * k, card.LowerRightCorner.X - 12 * k, card.UpperLeftCorner.Y + 46 * k),
        pal.text, HudFont::Left, 0.8f * k, &clip);
    fill(driver, irr::core::rect<irr::f32>(px, card.UpperLeftCorner.Y + 50 * k, card.LowerRightCorner.X - 14 * k, card.UpperLeftCorner.Y + 50 * k + std::max(1.0f, 1.5f * k)), pal.text, &clip);
    const std::vector<std::wstring> lines = fonts.infoText->wrap(help, card.getWidth() - 28 * k);
    irr::f32 ly = card.UpperLeftCorner.Y + 60 * k;
    for (size_t i = 0; i < lines.size() && ly + lh <= card.LowerRightCorner.Y; i++, ly += lh) {
        fonts.infoText->drawIn(lines[i], irr::core::rect<irr::f32>(px, ly, card.LowerRightCorner.X - 12 * k, ly + lh), pal.textDim, HudFont::Left, 0, &clip);
    }

    if (status.empty()) { return; }
    //State block: framed, like the picture tile of the weather window
    const irr::core::rect<irr::f32> box(info.UpperLeftCorner.X, info.LowerRightCorner.Y - statusH, info.LowerRightCorner.X, info.LowerRightCorner.Y);
    fill(driver, box, withAlpha(pal.edge, 240), &clip);
    const irr::f32 fr = 3 * k;
    const irr::core::rect<irr::f32> inner(box.UpperLeftCorner.X + fr, box.UpperLeftCorner.Y + fr, box.LowerRightCorner.X - fr, box.LowerRightCorner.Y - fr);
    bridge::fillRound(driver, toI(inner), 0, withAlpha(pal.field, 245), withAlpha(pal.field, 245), &clip);
    irr::core::rect<irr::s32> innerClip = toI(inner);
    innerClip.clipAgainst(clip);
    fonts.section->drawIn(L"\u00C9TAT ACTUEL", irr::core::rect<irr::f32>(inner.UpperLeftCorner.X + 12 * k, inner.UpperLeftCorner.Y + 6 * k, inner.LowerRightCorner.X, inner.UpperLeftCorner.Y + 26 * k),
        pal.textDim, HudFont::Left, 1.4f * k, &innerClip);
    irr::f32 ry = inner.UpperLeftCorner.Y + 34 * k;
    for (size_t i = 0; i < status.size(); i++) {
        const irr::video::SColor col = status[i].tone == 1 ? pal.ok : (status[i].tone == 2 ? pal.error : pal.text);
        const irr::f32 rx = inner.UpperLeftCorner.X + 12 * k;
        if (status[i].tone != 0) {
            irr::gui::PanelBatch b;
            b.begin(driver);
            b.disc(irr::core::vector2df(rx + 4 * k, ry + 9 * k), 4.5f * k, col, col);
            b.flush();
        }
        fonts.section->drawIn(hudUpper(status[i].label), irr::core::rect<irr::f32>(rx + (status[i].tone ? 16 * k : 0), ry, inner.LowerRightCorner.X - 10 * k, ry + 18 * k),
            pal.textDim, HudFont::Left, 1.2f * k, &innerClip);
        ry += 20 * k;
        for (size_t l = 0; l < statusLines[i].size(); l++, ry += lh) {
            fonts.infoText->drawIn(statusLines[i][l], irr::core::rect<irr::f32>(rx, ry, inner.LowerRightCorner.X - 10 * k, ry + lh), col, HudFont::Left, 0, &innerClip);
        }
        ry += 10 * k;
    }
}
