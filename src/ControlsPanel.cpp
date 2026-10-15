/*   NAUTITECH - Simulateur de Navigation
     The instructor's controls window (CONTROLES), drawn as an instructor station page.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "ControlsPanel.hpp"
#include "launcher/HudText.hpp"

#include <cmath>
#include <cwchar>
#include <algorithm>

using station::Rect;

namespace
{
    std::wstring textOf(irr::gui::IGUIElement* e)
    {
        return (e && e->getText()) ? std::wstring(e->getText()) : std::wstring();
    }
}

//=================================================================================================

ControlsPanel::ControlsPanel(irr::gui::IGUIEnvironment* environment, irr::gui::IGUIElement* parent, const irr::core::rect<irr::s32>& area)
    : IGUIElement(irr::gui::EGUIET_ELEMENT, environment, parent, -1, area),
    tab(0), hoveredRow(-1), selectedRow(-1), draggedRow(-1), hoveredSeg(-1), pressedRow(-1), pressedSeg(-1)
{
    frame.code = L"CTL 01";
    frame.title = L"Pupitre instructeur";
    frame.subtitle = L"Contr\u00F4les";
    setVisible(false);
}

ControlsPanel::~ControlsPanel()
{
}

void ControlsPanel::setTitle(const std::wstring& code, const std::wstring& name)
{
    frame.code = code;
    frame.subtitle = name;
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
    if (!tabs.empty()) { t.group = tabs.back().group; }
    tabs.push_back(t);
    station::Frame::Page page;
    page.name = name;
    page.group = t.group;
    frame.pages.push_back(page);
    return (int)tabs.size() - 1;
}

void ControlsPanel::setGroup(int t, const std::wstring& group)
{
    if (t < 0 || t >= (int)tabs.size()) { return; }
    //The tab and the ones after it that shared its heading
    const std::wstring old = tabs[t].group;
    for (int i = t; i < (int)tabs.size(); i++) {
        if (i > t && tabs[i].group != old) { break; }
        tabs[i].group = group;
        frame.pages[i].group = group;
    }
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
//Layout
//-------------------------------------------------------------------------------------------------

void ControlsPanel::updateAbsolutePosition()
{
    IGUIElement::updateAbsolutePosition();
    if (AbsoluteRect != laidOutRect) { layout(); }
}

void ControlsPanel::layout()
{
    laidOutRect = AbsoluteRect;
    frame.layout(Environment, AbsoluteRect, 340, 250);
    clampScroll();
}

Rect ControlsPanel::content() const
{
    const irr::f32 k = frame.k;
    const irr::f32 pad = (frame.compact ? 10 : 16) * k;
    return Rect(frame.main.UpperLeftCorner.X + pad, frame.main.UpperLeftCorner.Y + 4 * k,
        frame.main.LowerRightCorner.X - pad, frame.main.LowerRightCorner.Y - 6 * k);
}

irr::f32 ControlsPanel::rowHeight(const Row& row) const
{
    const irr::f32 k = frame.k;
    switch (row.kind) {
    case ROW_SECTION: return floorf(34 * k);
    case ROW_TEXT: return floorf(row.lines * (frame.fonts.text ? frame.fonts.text->lineHeight() + 3 * k : 22 * k) + 14 * k);
    case ROW_KEYS: return floorf((row.label.empty() ? 36 : 34) * k);
    default: return floorf(34 * k);
    }
}

Rect ControlsPanel::rowRect(int index) const
{
    if (tab < 0 || tab >= (int)tabs.size()) { return Rect(); }
    const std::vector<Row>& rs = tabs[tab].rows;
    const Rect area = content();
    irr::f32 y = area.UpperLeftCorner.Y - tabs[tab].scroll;
    for (int i = 0; i < index && i < (int)rs.size(); i++) { y += rowHeight(rs[i]); }
    const irr::f32 h = index < (int)rs.size() ? rowHeight(rs[index]) : 34 * frame.k;
    return Rect(area.UpperLeftCorner.X, y, area.LowerRightCorner.X - 8 * frame.k, y + h);
}

ControlsPanel::Cells ControlsPanel::cells(const Row& row, const Rect& r) const
{
    const irr::f32 k = frame.k;
    Cells c;
    const irr::f32 x0 = r.UpperLeftCorner.X + 12 * k, x1 = r.LowerRightCorner.X - 4 * k;
    const irr::f32 W = r.getWidth();
    if (row.kind == ROW_KEYS && row.label.empty()) {
        c.control = Rect(x0 - 4 * k, r.UpperLeftCorner.Y, x1, r.LowerRightCorner.Y);
        return c;
    }
    const irr::f32 labelW = W * (frame.compact ? 0.36f : 0.30f);
    const irr::f32 stateW = frame.compact ? 0 : W * 0.21f;
    c.label = Rect(x0, r.UpperLeftCorner.Y, r.UpperLeftCorner.X + labelW - 6 * k, r.LowerRightCorner.Y);
    c.state = Rect(r.UpperLeftCorner.X + labelW, r.UpperLeftCorner.Y, r.UpperLeftCorner.X + labelW + stateW, r.LowerRightCorner.Y);
    c.control = Rect(r.UpperLeftCorner.X + labelW + stateW, r.UpperLeftCorner.Y, x1, r.LowerRightCorner.Y);
    return c;
}

std::vector<std::wstring> ControlsPanel::segmentTexts(const Row& row) const
{
    std::vector<std::wstring> out;
    if (row.kind == ROW_KEYS) { out = row.names; }
    else if (row.kind == ROW_TOGGLE) { out.push_back(L"Non"); out.push_back(L"Oui"); }
    else if (row.kind == ROW_CHOICE && row.combo) {
        for (irr::u32 i = 0; i < row.combo->getItemCount(); i++) { out.push_back(row.combo->getItem(i)); }
    }
    return out;
}

bool ControlsPanel::usesSegments(const Row& row, const Cells& c) const
{
    if (row.kind == ROW_KEYS || row.kind == ROW_TOGGLE) { return true; }
    if (row.kind != ROW_CHOICE || !row.combo) { return false; }
    const irr::u32 n = row.combo->getItemCount();
    return n >= 2 && n <= 6 && station::segmentsFit(frame.fonts, frame.k, segmentTexts(row), c.control.getWidth());
}

std::vector<Rect> ControlsPanel::segmentRects(const Row& row, const Rect& r) const
{
    const Cells c = cells(row, r);
    if (!usesSegments(row, c)) { return std::vector<Rect>(); }
    const std::vector<std::wstring> texts = segmentTexts(row);
    int align = station::ALIGN_RIGHT;
    irr::f32 h = 24 * frame.k;
    if (row.kind == ROW_KEYS && row.label.empty()) {
        align = texts.size() > 1 ? station::ALIGN_FILL : station::ALIGN_LEFT;
        h = 26 * frame.k;
    }
    return station::layoutSegments(frame.fonts, frame.k, texts, c.control, align, h);
}

Rect ControlsPanel::stepperRect(const Cells& c) const
{
    const irr::f32 k = frame.k;
    const irr::f32 w = std::min(c.control.getWidth(), 300 * k);
    const irr::f32 cy = c.control.getCenter().Y;
    return Rect(floorf(c.control.LowerRightCorner.X - w), floorf(cy - 12 * k), floorf(c.control.LowerRightCorner.X), floorf(cy + 12 * k));
}

Rect ControlsPanel::sliderTrack(const Cells& c) const
{
    const irr::f32 k = frame.k;
    //Narrow: no state column, the value is written at the start of the cell
    const irr::f32 x0 = c.control.UpperLeftCorner.X + (frame.compact ? 76 : 14) * k;
    const irr::f32 cy = c.control.getCenter().Y;
    return Rect(x0, cy - 2 * k, c.control.LowerRightCorner.X - 10 * k, cy + 2 * k);
}

irr::f32 ControlsPanel::contentHeight() const
{
    if (tab < 0 || tab >= (int)tabs.size()) { return 0; }
    irr::f32 h = 0;
    for (size_t i = 0; i < tabs[tab].rows.size(); i++) { h += rowHeight(tabs[tab].rows[i]); }
    return h + 8 * frame.k;
}

void ControlsPanel::clampScroll()
{
    if (tab < 0 || tab >= (int)tabs.size()) { return; }
    const irr::f32 maxScroll = std::max(0.0f, contentHeight() - content().getHeight());
    tabs[tab].scroll = irr::core::clamp(tabs[tab].scroll, 0.0f, maxScroll);
}

int ControlsPanel::rowAt(const irr::core::vector2df& p) const
{
    if (tab < 0 || tab >= (int)tabs.size() || !content().isPointInside(p)) { return -1; }
    const std::vector<Row>& rs = tabs[tab].rows;
    for (int i = 0; i < (int)rs.size(); i++) {
        if (rs[i].kind == ROW_SECTION) { continue; }
        if (rowRect(i).isPointInside(p)) { return i; }
    }
    return -1;
}

int ControlsPanel::segmentAt(int row, const irr::core::vector2df& p) const
{
    if (row < 0 || tab < 0 || tab >= (int)tabs.size() || row >= (int)tabs[tab].rows.size()) { return -1; }
    const Row& r = tabs[tab].rows[row];
    const Rect rr = rowRect(row);
    const std::vector<Rect> segs = segmentRects(r, rr);
    for (int i = 0; i < (int)segs.size(); i++) {
        if (segs[i].isPointInside(p)) { return i; }
    }
    if (r.kind == ROW_CHOICE && segs.empty()) {
        //The stepper: -2 its left arrow, -3 its right arrow
        const Rect s = stepperRect(cells(r, rr));
        if (s.isPointInside(p)) { return p.X < s.getCenter().X ? -2 : -3; }
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
    const Rect track = sliderTrack(cells(row, rowRect(rowIndex)));
    irr::f32 t = ((irr::f32)mouseX - track.UpperLeftCorner.X) / std::max(1.0f, track.getWidth());
    t = irr::core::clamp(t, 0.0f, 1.0f);
    const irr::s32 lo = row.bar->getMin(), hi = row.bar->getMax();
    const irr::s32 v = lo + (irr::s32)floorf(t * (hi - lo) + 0.5f);
    if (v != row.bar->getPos()) {
        row.bar->setPos(v);
        send(row.bar, irr::gui::EGET_SCROLL_BAR_CHANGED);
    }
}

void ControlsPanel::recordAction(const Row& row, const std::wstring& value)
{
    const std::wstring what = row.label.empty() ? (tab >= 0 && tab < (int)tabs.size() ? tabs[tab].name : std::wstring()) : row.label;
    lastAction = what + L" \u00BB " + value;
    if (clockText) {
        const std::wstring t = clockText();
        if (!t.empty()) { lastAction += L"  \u00B7  " + t; }
    }
}

std::wstring ControlsPanel::stateText(const Row& row, int& tone) const
{
    tone = station::TONE_NONE;
    switch (row.kind) {
    case ROW_KEYS: {
        const int s = row.selected ? row.selected() : -1;
        if (s < 0 || s >= (int)row.names.size()) { return std::wstring(); }
        tone = station::toneOfWords(row.names[s]);
        return row.names[s];
    }
    case ROW_TOGGLE:
        if (!row.box->isEnabled()) { return L"Absent"; }
        return row.box->isChecked() ? L"Oui" : L"Non";
    case ROW_SLIDER:
        return textOf(row.text);
    case ROW_CHOICE: {
        const int n = (int)row.combo->getItemCount();
        if (n <= 0) { return std::wstring(); }
        return row.combo->getItem(irr::core::clamp(row.combo->getSelected(), 0, n - 1));
    }
    default:
        return std::wstring();
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
    if (draggedRow < 0 && (dock.isDragging() || AbsoluteRect.isPointInside(p)) &&
        dock.onMouse(this, Environment, m, frame.header, frame.dockRect(dock.place), frame.closeRect())) {
        return true;
    }
    switch (m.Event) {
    case irr::EMIE_MOUSE_MOVED:
        if (draggedRow >= 0) { dragSlider(draggedRow, m.X); return true; }
        frame.hover(pf);
        hoveredRow = rowAt(pf);
        hoveredSeg = segmentAt(hoveredRow, pf);
        return AbsoluteRect.isPointInside(p);
    case irr::EMIE_LMOUSE_PRESSED_DOWN: {
        if (!AbsoluteRect.isPointInside(p)) { return false; }
        if (frame.closeRect().isPointInside(pf)) {
            setVisible(false);
            Environment->removeFocus(this);
            return true;
        }
        const int page = frame.pageAt(pf);
        if (page >= 0) {
            tab = page;
            selectedRow = hoveredRow = hoveredSeg = -1;
            clampScroll();
            return true;
        }
        const int row = rowAt(pf);
        if (row < 0) { return true; }
        selectedRow = row;
        const Row& r = tabs[tab].rows[row];
        const Rect rr = rowRect(row);
        const Cells c = cells(r, rr);
        const int seg = segmentAt(row, pf);
        if (r.kind == ROW_TOGGLE) {
            //A side of the switch, or anywhere else on the row to change it over
            const bool want = seg >= 0 ? (seg == 1) : !r.box->isChecked();
            if (r.box->isEnabled() && want != r.box->isChecked()) {
                r.box->setChecked(want);
                send(r.box, irr::gui::EGET_CHECKBOX_CHANGED);
                recordAction(r, want ? L"Oui" : L"Non");
            }
        }
        else if (r.kind == ROW_SLIDER && c.control.isPointInside(pf)) {
            draggedRow = row;
            Environment->setFocus(this); //the moves come here while dragging, even off the row
            dragSlider(row, m.X);
        }
        else if (r.kind == ROW_KEYS) {
            if (seg >= 0 && seg < (int)r.keys.size() && r.keys[seg]->isEnabled()) {
                pressedRow = row;
                pressedSeg = seg;
                send(r.keys[seg], irr::gui::EGET_BUTTON_CLICKED);
                recordAction(r, r.names[seg]);
            }
        }
        else if (r.kind == ROW_CHOICE && r.combo->getItemCount() > 0) {
            const int n = (int)r.combo->getItemCount();
            const int current = irr::core::clamp(r.combo->getSelected(), 0, n - 1);
            int next = current;
            if (seg >= 0) { next = seg; }
            else if (seg == -2) { next = (current + n - 1) % n; }
            else { next = (current + 1) % n; }  //right arrow, or the label
            if (next != current) {
                r.combo->setSelected(next);
                send(r.combo, irr::gui::EGET_COMBO_BOX_CHANGED);
                recordAction(r, r.combo->getItem(next));
            }
        }
        return true;
    }
    case irr::EMIE_LMOUSE_LEFT_UP:
        pressedRow = pressedSeg = -1;
        if (draggedRow >= 0) {
            if (tab < (int)tabs.size() && draggedRow < (int)tabs[tab].rows.size()) {
                recordAction(tabs[tab].rows[draggedRow], textOf(tabs[tab].rows[draggedRow].text));
            }
            draggedRow = -1;
            Environment->removeFocus(this); //keys go back to the ship
            return true;
        }
        return AbsoluteRect.isPointInside(p);
    case irr::EMIE_MOUSE_WHEEL:
        if (!AbsoluteRect.isPointInside(p)) { return false; }
        if (frame.main.isPointInside(pf) && tab >= 0 && tab < (int)tabs.size()) {
            tabs[tab].scroll -= m.Wheel * 64.0f * frame.k;
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
    if (!frame.fonts.title) { return; }
    if (tab >= (int)tabs.size()) { tab = 0; }
    irr::video::IVideoDriver* driver = Environment->getVideoDriver();
    const irr::core::rect<irr::s32>& clip = AbsoluteClippingRect;

    //Faults of each page, beside its name
    std::vector<Readout> status;
    for (int t = 0; t < (int)tabs.size(); t++) {
        std::vector<Readout> st;
        if (tabs[t].status) { st = tabs[t].status(); }
        int faults = 0;
        for (size_t i = 0; i < st.size(); i++) { if (st[i].tone == station::TONE_ERR) { faults++; } }
        frame.pages[t].badge = faults;
        if (t == tab) { status = st; }
    }

    std::vector<station::Status> head;
    if (headerStatus) {
        const std::vector<Readout> hs = headerStatus();
        for (size_t i = 0; i < hs.size(); i++) { head.push_back({ hs[i].label, hs[i].text, hs[i].tone }); }
    }
    frame.drawBack(driver, dock, tab, head, clip);

    //The rows (clipped to the page, they scroll)
    const Rect area = content();
    const irr::core::rect<irr::s32> pageClip = station::clipOf(Rect(frame.main.UpperLeftCorner.X, area.UpperLeftCorner.Y, frame.main.LowerRightCorner.X, area.LowerRightCorner.Y), clip);
    drawRows(driver, pageClip);
    if (tab < (int)tabs.size()) {
        station::drawScrollBar(driver, frame.k, Rect(area.UpperLeftCorner.X, area.UpperLeftCorner.Y, area.LowerRightCorner.X + 6 * frame.k, area.LowerRightCorner.Y),
            contentHeight(), tabs[tab].scroll, clip);
    }

    drawSide(driver, status, clip);

    std::vector<station::Status> foot;
    foot.push_back({ frame.code, L"", 0 });
    if (tab < (int)tabs.size()) { foot.push_back({ L"Onglet", tabs[tab].name, 0 }); }
    foot.push_back({ L"Derni\u00E8re action", lastAction.empty() ? std::wstring(L"\u2014") : lastAction, 0 });
    frame.drawFooter(driver, foot, L"Molette : d\u00E9filer  \u00B7  glisser le titre : d\u00E9placer", clip);
}

void ControlsPanel::drawRows(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip)
{
    if (tab < 0 || tab >= (int)tabs.size()) { return; }
    const station::Colours& col = station::colours();
    const station::Fonts& f = frame.fonts;
    const irr::f32 k = frame.k;
    const Rect area = content();
    const std::vector<Row>& rs = tabs[tab].rows;
    int odd = 0;
    for (int i = 0; i < (int)rs.size(); i++) {
        const Row& row = rs[i];
        const Rect r = rowRect(i);
        if (row.kind == ROW_SECTION) { odd = 0; }
        const bool visible = !(r.LowerRightCorner.Y < area.UpperLeftCorner.Y || r.UpperLeftCorner.Y > area.LowerRightCorner.Y);

        if (row.kind == ROW_SECTION) {
            if (!visible) { continue; }
            //How many items until the next section
            int count = 0;
            for (int j = i + 1; j < (int)rs.size() && rs[j].kind != ROW_SECTION; j++) { if (rs[j].kind != ROW_TEXT) { count++; } }
            std::wstring items;
            if (count > 1) { items = std::to_wstring(count) + L" \u00C9L\u00C9MENTS"; }
            station::drawSection(driver, f, k, Rect(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y - 4 * k), row.label, items, clip);
            continue;
        }
        if (row.kind == ROW_TEXT) {
            if (!visible) { continue; }
            //Read-only: what the simulation says, on a plain field with a bar when it is an alert
            const Rect box(r.UpperLeftCorner.X, r.UpperLeftCorner.Y + 4 * k, r.LowerRightCorner.X, r.LowerRightCorner.Y - 4 * k);
            const bool alert = row.alert && row.alert();
            station::fill(driver, box, col.key, &clip);
            station::frame(driver, box, col.edge2, &clip);
            if (alert) { station::fill(driver, Rect(box.UpperLeftCorner.X, box.UpperLeftCorner.Y, box.UpperLeftCorner.X + 3 * k, box.LowerRightCorner.Y), col.err, &clip); }
            const std::vector<std::wstring> lines = f.text->wrap(textOf(row.text), box.getWidth() - 28 * k);
            const irr::f32 lh = f.text->lineHeight() + 3 * k;
            irr::f32 ly = box.UpperLeftCorner.Y + 4 * k;
            for (size_t l = 0; l < lines.size() && (int)l < row.lines; l++, ly += lh) {
                f.text->drawIn(lines[l], Rect(box.UpperLeftCorner.X + 14 * k, ly, box.LowerRightCorner.X - 10 * k, ly + lh), alert ? col.err : col.text, HudFont::Left, 0, &clip);
            }
            continue;
        }
        const bool shade = (odd++ % 2) == 0;
        if (!visible) { continue; }

        const bool active = (i == selectedRow) || (i == draggedRow);
        const bool hover = (i == hoveredRow);
        const bool enabled = !(row.kind == ROW_TOGGLE && !row.box->isEnabled());
        station::drawRowBack(driver, k, r, shade, hover, active, clip);
        const Cells c = cells(row, r);

        //Label
        if (!(row.kind == ROW_KEYS && row.label.empty())) {
            const irr::core::rect<irr::s32> lc = station::clipOf(c.label, clip);
            f.label->drawIn(row.label, Rect(c.label.UpperLeftCorner.X, c.label.UpperLeftCorner.Y, c.label.LowerRightCorner.X, c.label.LowerRightCorner.Y + 1 * k),
                enabled ? col.text : col.faint, HudFont::Left, 0.5f * k, &lc);
        }

        //State
        int tone = station::TONE_NONE;
        const std::wstring state = stateText(row, tone);
        if (c.state.getWidth() > 0 && !state.empty()) {
            if (row.kind == ROW_SLIDER || row.kind == ROW_CHOICE) {
                const irr::core::rect<irr::s32> sc = station::clipOf(c.state, clip);
                f.state->drawIn(hudUpper(state), Rect(c.state.UpperLeftCorner.X, c.state.UpperLeftCorner.Y, c.state.LowerRightCorner.X - 6 * k, c.state.LowerRightCorner.Y + 1 * k),
                    col.text, HudFont::Left, 1.0f * k, &sc);
            }
            else if (row.kind == ROW_TOGGLE) {
                const bool on = enabled && row.box->isChecked();
                if (on) { station::lamp(driver, c.state.UpperLeftCorner.X + 4 * k, c.state.getCenter().Y, 7 * k, col.accent, &clip); }
                const irr::f32 x = c.state.UpperLeftCorner.X + (on ? 14 * k : 0);
                f.state->drawIn(hudUpper(state), Rect(x, c.state.UpperLeftCorner.Y, c.state.LowerRightCorner.X, c.state.LowerRightCorner.Y + 1 * k),
                    on ? col.accent : (enabled ? col.dim : col.faint), HudFont::Left, 1.4f * k, &clip);
            }
            else {
                station::drawState(driver, f, k, Rect(c.state.UpperLeftCorner.X, c.state.UpperLeftCorner.Y, c.state.LowerRightCorner.X - 6 * k, c.state.LowerRightCorner.Y),
                    state, tone, true, clip);
            }
        }

        //Control
        if (row.kind == ROW_SLIDER) {
            const Rect track = sliderTrack(c);
            if (frame.compact) {
                f.state->drawIn(hudUpper(state), Rect(c.control.UpperLeftCorner.X, c.control.UpperLeftCorner.Y, track.UpperLeftCorner.X - 8 * k, c.control.LowerRightCorner.Y + 1 * k),
                    col.text, HudFont::Left, 1.0f * k, &clip);
            }
            const irr::s32 lo = row.bar->getMin(), hi = row.bar->getMax();
            const irr::f32 t = (irr::f32)(row.bar->getPos() - lo) / (irr::f32)std::max(1, hi - lo);
            station::drawFader(driver, k, track, t, active || hover, clip);
            continue;
        }
        const std::vector<Rect> segs = segmentRects(row, r);
        if (!segs.empty()) {
            const std::vector<std::wstring> texts = segmentTexts(row);
            int on = -1;
            if (row.kind == ROW_KEYS && row.selected) { on = row.selected(); }
            if (row.kind == ROW_TOGGLE) { on = row.box->isChecked() ? 1 : 0; }
            if (row.kind == ROW_CHOICE) { on = row.combo->getSelected(); }
            std::vector<station::Segment> ss(segs.size());
            for (size_t j = 0; j < segs.size(); j++) {
                ss[j].text = texts[j];
                ss[j].on = ((int)j == on);
                ss[j].tone = (row.kind == ROW_KEYS) ? station::toneOfWords(texts[j]) : (int)station::TONE_NONE;
                if (ss[j].tone == station::TONE_OK) { ss[j].tone = station::TONE_NONE; }
                ss[j].hover = (i == hoveredRow && (int)j == hoveredSeg);
                ss[j].down = (i == pressedRow && (int)j == pressedSeg);
                ss[j].enabled = enabled && !(row.kind == ROW_KEYS && !row.keys[j]->isEnabled());
            }
            station::drawSegments(driver, f, k, segs, ss, clip);
        }
        else if (row.kind == ROW_CHOICE) {
            const int side = (i == hoveredRow) ? (hoveredSeg == -2 ? -1 : (hoveredSeg == -3 ? 1 : 0)) : 0;
            station::drawStepper(driver, f, k, stepperRect(c), state, side, clip);
        }
    }
}

void ControlsPanel::drawSide(irr::video::IVideoDriver* driver, const std::vector<Readout>& status, const irr::core::rect<irr::s32>& clip)
{
    if (tab < 0 || tab >= (int)tabs.size()) { return; }
    const station::Colours& col = station::colours();
    const station::Fonts& f = frame.fonts;
    const irr::f32 k = frame.k;
    const Tab& t = tabs[tab];

    //What the explanation is about: the row (or key) under the mouse, else the one last used, else the tab
    std::wstring title = t.name, help = t.help;
    const int r = hoveredRow >= 0 ? hoveredRow : selectedRow;
    if (r >= 0 && r < (int)t.rows.size() && t.rows[r].kind != ROW_TEXT) {
        const Row& row = t.rows[r];
        const int key = (r == hoveredRow && row.kind == ROW_KEYS) ? hoveredSeg : -1;
        if (key >= 0 && key < (int)row.keys.size()) {
            title = row.label.empty() ? row.names[key] : row.label + L" \u2014 " + row.names[key];
            help.clear();
            if (row.keys[key]->getToolTipText().size() > 0) { help = row.keys[key]->getToolTipText().c_str(); }
            if (help.empty()) { help = row.help; }
        }
        else {
            int tone = 0;
            const std::wstring state = stateText(row, tone);
            title = row.label.empty() ? t.name : row.label;
            if (!state.empty() && row.kind != ROW_SLIDER) { title += L" \u2014 " + state; }
            help = row.help;
            if (help.empty()) {
                irr::gui::IGUIElement* w = row.bar ? (irr::gui::IGUIElement*)row.bar : row.box ? (irr::gui::IGUIElement*)row.box : (irr::gui::IGUIElement*)row.combo;
                if (w) { help = w->getToolTipText().c_str(); }
            }
        }
    }

    const irr::f32 pad = (frame.compact ? 10 : 14) * k;
    const Rect area(frame.side.UpperLeftCorner.X + pad, frame.side.UpperLeftCorner.Y + pad, frame.side.LowerRightCorner.X - pad, frame.side.LowerRightCorner.Y - pad);
    Rect stateBox, detailBox = area;
    if (frame.compact && !status.empty()) {
        //Side by side under the page
        const irr::f32 split = area.UpperLeftCorner.X + area.getWidth() * 0.54f;
        stateBox = Rect(area.UpperLeftCorner.X, area.UpperLeftCorner.Y, split - pad * 0.5f, area.LowerRightCorner.Y);
        detailBox = Rect(split + pad * 0.5f, area.UpperLeftCorner.Y, area.LowerRightCorner.X, area.LowerRightCorner.Y);
    }
    else if (!status.empty()) {
        stateBox = Rect(area.UpperLeftCorner.X, area.UpperLeftCorner.Y, area.LowerRightCorner.X, area.UpperLeftCorner.Y);
    }

    //State of the page: label on the left, value on the right (under it when it is long)
    if (!status.empty()) {
        const irr::f32 w = stateBox.getWidth();
        const irr::f32 lh = f.strong->lineHeight() + 2 * k;
        std::vector<std::vector<std::wstring> > lines(status.size());
        std::vector<bool> stacked(status.size());
        std::vector<irr::f32> heights(status.size());
        irr::f32 total = station::boxHeaderHeight(k) + 4 * k;
        for (size_t i = 0; i < status.size(); i++) {
            const std::wstring label = hudUpper(status[i].label), value = hudUpper(status[i].text);
            const irr::f32 lw = f.key->width(label, 1.0f * k), vw = f.strong->width(value, 1.0f * k);
            const irr::f32 lampW = status[i].tone != 0 ? 14 * k : 0;
            stacked[i] = lw + vw + lampW + 34 * k > w;
            if (stacked[i]) {
                lines[i] = f.strong->wrap(value, w - 20 * k - lampW, 1.0f * k);
                if (lines[i].empty()) { lines[i].push_back(L"\u2014"); }
                heights[i] = 26 * k + lines[i].size() * lh + 6 * k;
            }
            else {
                lines[i].push_back(value);
                heights[i] = 32 * k;
            }
            total += heights[i];
        }
        if (!frame.compact) {
            total = std::min(total, area.getHeight() * 0.62f);
            stateBox.LowerRightCorner.Y = stateBox.UpperLeftCorner.Y + total;
            detailBox.UpperLeftCorner.Y = stateBox.LowerRightCorner.Y + pad;
        }
        const Rect in = station::drawBox(driver, f, k, stateBox, L"\u00C9tat \u00B7 " + t.name, clip);
        const irr::core::rect<irr::s32> ic = station::clipOf(in, clip);
        irr::f32 y = in.UpperLeftCorner.Y;
        for (size_t i = 0; i < status.size(); i++) {
            const irr::f32 x0 = in.UpperLeftCorner.X + 10 * k, x1 = in.LowerRightCorner.X - 10 * k;
            const irr::video::SColor tc = status[i].tone == station::TONE_NONE ? col.text : station::toneColour(status[i].tone);
            if (!stacked[i]) {
                const Rect rr(x0, y, x1, y + heights[i] + 1 * k);
                f.key->drawIn(hudUpper(status[i].label), rr, col.dim, HudFont::Left, 1.0f * k, &ic);
                const irr::f32 vw = f.strong->width(lines[i][0], 1.0f * k);
                f.strong->drawIn(lines[i][0], rr, tc, HudFont::Right, 1.0f * k, &ic);
                if (status[i].tone != 0) { station::lamp(driver, x1 - vw - 10 * k, y + heights[i] * 0.5f, 7 * k, tc, &ic); }
            }
            else {
                f.key->drawIn(hudUpper(status[i].label), Rect(x0, y + 4 * k, x1, y + 24 * k), col.dim, HudFont::Left, 1.0f * k, &ic);
                irr::f32 ly = y + 24 * k;
                const irr::f32 lx = x0 + (status[i].tone != 0 ? 14 * k : 0);
                if (status[i].tone != 0) { station::lamp(driver, x0 + 4 * k, ly + lh * 0.5f, 7 * k, tc, &ic); }
                for (size_t l = 0; l < lines[i].size(); l++, ly += lh) {
                    f.strong->drawIn(lines[i][l], Rect(lx, ly, x1, ly + lh), tc, HudFont::Left, 1.0f * k, &ic);
                }
            }
            y += heights[i];
            //Dashed rule between readouts
            if (i + 1 < status.size()) {
                for (irr::f32 dx = in.UpperLeftCorner.X; dx < in.LowerRightCorner.X; dx += 6 * k) {
                    station::hline(driver, dx, std::min(dx + 3 * k, in.LowerRightCorner.X), y - 1, col.rule, &ic);
                }
            }
        }
    }

    //Explanation
    if (detailBox.getHeight() > 40 * k) {
        const irr::f32 need = station::boxHeaderHeight(k) + station::detailHeight(f, k, detailBox.getWidth(), title, help);
        if (!frame.compact && need < detailBox.getHeight()) { detailBox.LowerRightCorner.Y = detailBox.UpperLeftCorner.Y + need; }
        const Rect in = station::drawBox(driver, f, k, detailBox, L"D\u00E9tail", clip);
        station::drawDetail(driver, f, k, in, title, help, clip);
    }
}
