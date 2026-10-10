/*   NAUTITECH - Simulateur de Navigation
     The multiplayer hub's live screen: the instructor's master control (see InstructorStation.hpp).

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "InstructorStation.hpp"
#include "../UiTheme.hpp"
#include "../chartView/ChartView.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace {
    const irr::f32 TRACK_INTERVAL = 5.0f;   //seconds of exercise time between track points
    const size_t TRACK_POINTS = 1440;       //two hours

    std::string num(irr::f32 v)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.3f", v);
        return buf;
    }
}

InstructorStation::InstructorStation(irr::IrrlichtDevice* device, bool french, const std::wstring& exercise, irr::u32 stationCount,
    const std::string& worldName, const ScenarioWeather& scenarioWeather,
    irr::gui::IGUIFont* bigFont, irr::gui::IGUIFont* titleFont, irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* smallFont)
    : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, device->getGUIEnvironment(), device->getGUIEnvironment()->getRootGUIElement(), -1,
        irr::core::rect<irr::s32>(irr::core::position2di(0, 0), device->getVideoDriver()->getScreenSize())),
    device(device), french(french), exercise(exercise), stationCount(stationCount),
    bigFont(bigFont), titleFont(titleFont), textFont(textFont), smallFont(smallFont), scenarioWeather(scenarioWeather),
    messageBox(0), running(false), accelerator(0), scenarioTime(0), lines(0),
    chart(0), chartLoaded(false), chartFittedShips(0), chartTouched(false), dragging(false), dragMoved(false),
    selected(-1), weatherPreset(-1), windDirIndex(0), transitionIndex(1), hourIndex(0), delayIndex(0)
{
    //The same presets as the simulator's weather window:
    //name, cloud, wind, variation, gusts, visibility, rain, snow, dust, sea, significant
    auto add = [&](const wchar_t* name, irr::f32 cloud, irr::f32 wind, irr::f32 variation, irr::f32 gusts, irr::f32 visibility,
        irr::f32 rain, irr::f32 snow, irr::f32 dust, irr::f32 sea, int significant) {
        Preset p;
        p.name = name; p.cloud = cloud; p.windKn = wind; p.windVariation = variation; p.gustKn = gusts; p.visibilityNm = visibility;
        p.rain = rain; p.snow = snow; p.dust = dust; p.sea = sea; p.significant = significant;
        presets.push_back(p);
    };
    add(L"Clair", 0.0f, 6, 10, 2, 12, 0, 0, 0, 0.5f, 1);
    add(L"Peu nuageux", 0.25f, 9, 12, 3, 10, 0, 0, 0, 1.0f, 1);
    add(L"Nuageux", 0.55f, 13, 15, 5, 8, 0, 0, 0, 1.5f, 1);
    add(L"Couvert", 0.9f, 16, 15, 6, 6, 0, 0, 0, 2.0f, 1);
    add(L"Pluie faible", 0.85f, 13, 15, 6, 4, 2.5f, 0, 0, 1.8f, 1);
    add(L"Pluie", 1.0f, 19, 20, 8, 2.5f, 6, 0, 0, 2.5f, 1);
    add(L"Brouillard", 0.7f, 3, 20, 0, 0.12f, 0, 0, 0, 0.5f, 1);
    add(L"Brume", 0.6f, 6, 15, 2, 1.0f, 0, 0, 0, 0.8f, 1);
    add(L"Neige", 1.0f, 14, 20, 6, 2, 0, 0.75f, 0, 2.0f, 1);
    add(L"Orage", 1.0f, 26, 30, 15, 2, 8, 0, 0, 3.5f, 2);
    add(L"Grains", 0.8f, 18, 25, 10, 5, 2, 0, 0, 2.5f, 3);
    add(L"Sable", 0.3f, 16, 20, 6, 3, 0, 0, 0.45f, 1.5f, 1);
    add(L"Coup de vent", 0.8f, 38, 20, 12, 5, 2, 0, 0, 4.5f, 1);
    add(L"Temp\u00EAte", 1.0f, 55, 30, 18, 1.5f, 7, 0, 0, 6.0f, 2);

    //The wind's direction starts as the scenario's, to the nearest point of the compass
    windDirIndex = ((int)std::floor(scenarioWeather.windDir / 45.0f + 0.5f)) % 8;
    if (windDirIndex < 0) { windDirIndex += 8; }

    layout();

    //The live chart of the exercise's area
    chart = new ChartView();
    std::string error;
    chartLoaded = chart->load(device, worldName, error);
    if (chartLoaded) {
        chart->setViewport(Ui::toI(chartArea));
        chart->setStyle(ChartView::Style_Day);
        chart->fitWorld();
    }
    else {
        std::cout << "Instructor station: no chart for " << worldName << ": " << error << std::endl;
    }

    //Message to the students
    messageBox = Environment->addEditBox(L"", irr::core::rect<irr::s32>(0, 0, 10, 10), true, this);
    messageBox->setMax(120);
    if (smallFont) { messageBox->setOverrideFont(smallFont); }
    layout();
}

InstructorStation::~InstructorStation()
{
    delete chart;
}

void InstructorStation::layout()
{
    const irr::f32 W = (irr::f32)AbsoluteRect.getWidth(), H = (irr::f32)AbsoluteRect.getHeight();
    const irr::f32 top = 112, bottom = H - 40;
    const irr::f32 leftW = 340, rightW = 450;
    clockCard = irr::core::rect<irr::f32>(20, top, 20 + leftW, top + 220);
    weatherCard = irr::core::rect<irr::f32>(20, clockCard.LowerRightCorner.Y + 12, 20 + leftW, bottom);
    const irr::f32 stationsH = irr::core::clamp(70.0f + 48.0f * (stationCount + 1), 170.0f, 410.0f);
    stationCard = irr::core::rect<irr::f32>(W - 20 - rightW, top, W - 20, top + stationsH);
    failureCard = irr::core::rect<irr::f32>(W - 20 - rightW, stationCard.LowerRightCorner.Y + 12, W - 20, bottom);
    chartCard = irr::core::rect<irr::f32>(20 + leftW + 16, top, W - 20 - rightW - 16, bottom);
    chartArea = irr::core::rect<irr::f32>(chartCard.UpperLeftCorner.X + 10, chartCard.UpperLeftCorner.Y + 48,
        chartCard.LowerRightCorner.X - 10, chartCard.LowerRightCorner.Y - 10);
    const irr::f32 bx0 = clockCard.UpperLeftCorner.X + 20, bx1 = clockCard.LowerRightCorner.X - 20, mid = clockCard.getCenter().X;
    run = irr::core::rect<irr::f32>(bx0, clockCard.LowerRightCorner.Y - 64, mid - 6, clockCard.LowerRightCorner.Y - 20);
    pause = irr::core::rect<irr::f32>(mid + 6, clockCard.LowerRightCorner.Y - 64, bx1, clockCard.LowerRightCorner.Y - 20);
    if (messageBox) {
        const irr::f32 y = failureCard.UpperLeftCorner.Y + 470;
        messageBox->setRelativePosition(Ui::toI(irr::core::rect<irr::f32>(failureCard.UpperLeftCorner.X + 20, y,
            failureCard.LowerRightCorner.X - 120, y + 32)));
    }
}

irr::core::rect<irr::s32> InstructorStation::runRect() const { return Ui::toI(run); }
irr::core::rect<irr::s32> InstructorStation::pauseRect() const { return Ui::toI(pause); }

void InstructorStation::update(const std::wstring& clockText, const std::wstring& dateText, irr::f32 acceleratorNow, irr::f32 scenarioTimeNow,
    const std::vector<Station>& stationsNow, irr::u32 linesNow)
{
    clock = clockText;
    date = dateText;
    accelerator = acceleratorNow;
    running = acceleratorNow > 0;
    scenarioTime = scenarioTimeNow;
    stations = stationsNow;
    lines = linesNow;
    if (selected >= (int)stations.size()) { selected = -1; }

    //Tracks: a point every few seconds of exercise time
    tracks.resize(stations.size());
    lastTrackTime.resize(stations.size(), -1.0e9f);
    for (size_t i = 0; i < stations.size(); i++) {
        if (!stations[i].reported) { continue; }
        if (scenarioTime < lastTrackTime[i]) { lastTrackTime[i] = -1.0e9f; } //(the clock went back)
        if (scenarioTime - lastTrackTime[i] >= TRACK_INTERVAL) {
            tracks[i].push_back(irr::core::vector2df(stations[i].x, stations[i].z));
            if (tracks[i].size() > TRACK_POINTS) { tracks[i].pop_front(); }
            lastTrackTime[i] = scenarioTime;
        }
    }
    //As the students' ships appear, the chart frames them (until the instructor moves it)
    if (chartLoaded && !chartTouched) {
        int known = 0;
        for (size_t i = 0; i < stations.size(); i++) { if (stations[i].reported) { known++; } }
        if (known > chartFittedShips) { fitChartToShips(); chartFittedShips = known; }
    }
}

std::vector<InstructorStation::Command> InstructorStation::takeCommands()
{
    std::vector<Command> out;
    out.swap(commands);
    return out;
}

void InstructorStation::fitChartToShips()
{
    if (!chartLoaded) { return; }
    irr::f32 x0 = 1e30f, z0 = 1e30f, x1 = -1e30f, z1 = -1e30f;
    int n = 0;
    for (size_t i = 0; i < stations.size(); i++) {
        if (!stations[i].reported) { continue; }
        x0 = std::min(x0, stations[i].x); x1 = std::max(x1, stations[i].x);
        z0 = std::min(z0, stations[i].z); z1 = std::max(z1, stations[i].z);
        n++;
    }
    if (n == 0) { chart->fitWorld(); return; }
    chart->centreOnXZ((x0 + x1) * 0.5, (z0 + z1) * 0.5);
    const double w = std::max(1.0f, chartArea.getWidth()), h = std::max(1.0f, chartArea.getHeight());
    const double span = std::max((x1 - x0) / w, (z1 - z0) / h);
    chart->setMetresPerPixel(std::max(span * 1.6, 3000.0 / std::min(w, h))); //at least 3 km across
}

irr::video::SColor InstructorStation::stationColour(size_t i) const
{
    static const irr::video::SColor palette[] = {
        irr::video::SColor(255, 255, 196, 64), irr::video::SColor(255, 80, 220, 140), irr::video::SColor(255, 255, 110, 200),
        irr::video::SColor(255, 90, 200, 255), irr::video::SColor(255, 255, 140, 70), irr::video::SColor(255, 190, 150, 255),
        irr::video::SColor(255, 230, 230, 90), irr::video::SColor(255, 120, 255, 230) };
    return palette[i % 8];
}

std::wstring InstructorStation::targetName() const
{
    if (selected < 0 || selected >= (int)stations.size()) { return french ? L"tous les postes" : L"all stations"; }
    return stations[selected].ship.empty() ? stations[selected].host : stations[selected].ship;
}

//-------------------------------------------------------------------------------------------------
//Drawing
//-------------------------------------------------------------------------------------------------

void InstructorStation::label(irr::gui::IGUIFont* font, const std::wstring& text, const irr::core::rect<irr::f32>& r, irr::video::SColor col,
    int align, bool clip)
{
    Label l;
    l.font = font; l.text = text; l.r = r; l.col = col; l.align = align; l.clip = clip;
    labels.push_back(l);
}

//A clickable chip. tone: 0 normal, 1 a failure (red when selected), 2 degraded (amber when selected)
void InstructorStation::chip(irr::gui::PanelBatch& b, const irr::core::rect<irr::f32>& r, const std::wstring& text, bool selectedChip,
    ZoneKind kind, int value, int tone)
{
    const bool hover = r.isPointInside(irr::core::vector2df((irr::f32)mouse.X, (irr::f32)mouse.Y));
    irr::video::SColor fill = hover ? irr::video::SColor(255, 34, 60, 96) : irr::video::SColor(255, 22, 40, 66);
    irr::video::SColor ink = Ui::textDim;
    if (selectedChip) {
        fill = tone == 1 ? Ui::danger : (tone == 2 ? irr::video::SColor(255, 214, 140, 30) : Ui::accent);
        ink = Ui::text;
    }
    Ui::roundRect(b, r, 6, fill, Ui::mix(fill, irr::video::SColor(255, 0, 0, 0), 0.15f));
    Ui::roundRectOutline(b, r, 6, 1.0f, selectedChip ? Ui::alpha(Ui::text, 90) : Ui::edge);
    label(smallFont, text, r, ink, 1, true);
    Zone z;
    z.r = r; z.kind = kind; z.value = value;
    zones.push_back(z);
}

void InstructorStation::draw()
{
    if (!IsVisible) { return; }
    irr::video::IVideoDriver* driver = Environment->getVideoDriver();
    const irr::f32 W = (irr::f32)AbsoluteRect.getWidth(), H = (irr::f32)AbsoluteRect.getHeight();
    zones.clear();
    labels.clear();

    //Background, header and cards
    driver->draw2DRectangle(irr::core::rect<irr::s32>(0, 0, (irr::s32)W, (irr::s32)H), irr::video::SColor(255, 16, 32, 58),
        irr::video::SColor(255, 16, 32, 58), Ui::backgroundDeep, Ui::backgroundDeep);
    const std::wstring state = running ? (french ? L"EN COURS" : L"RUNNING") : (french ? L"EN PAUSE" : L"PAUSED");
    const irr::video::SColor stateCol = running ? Ui::success : Ui::warning;
    const irr::f32 pw = Ui::textWidth(smallFont, state) + 44;
    const irr::core::rect<irr::f32> pill(W - 28 - pw, 34, W - 28, 66);
    {
        irr::gui::PanelBatch b;
        b.begin(driver);
        b.disc(irr::core::vector2df(56, 56), 26, irr::video::SColor(70, 64, 156, 240), irr::video::SColor(70, 64, 156, 240));
        Ui::networkIcon(b, irr::core::vector2df(56, 56), 14, Ui::accentHi);
        b.rect(irr::core::rect<irr::f32>(28, 96, W - 28, 97), Ui::rule);
        Ui::roundRect(b, pill, 16, Ui::alpha(stateCol, 40), Ui::alpha(stateCol, 40));
        Ui::roundRectOutline(b, pill, 16, 1.0f, Ui::alpha(stateCol, 160));
        b.disc(irr::core::vector2df(pill.UpperLeftCorner.X + 18, pill.getCenter().Y), 5, stateCol, stateCol);
        Ui::card(b, clockCard, 14);
        Ui::card(b, weatherCard, 14);
        Ui::card(b, stationCard, 14);
        Ui::card(b, failureCard, 14);
        Ui::card(b, chartCard, 14);
        b.flush();
    }

    //The chart itself (its own drawing), then everything over it in one batch
    if (chartLoaded) {
        chart->setViewport(Ui::toI(chartArea));
        chart->draw(driver);
    }
    else {
        driver->draw2DRectangle(irr::video::SColor(255, 10, 26, 46), Ui::toI(chartArea));
    }
    irr::gui::PanelBatch b;
    b.begin(driver);
    drawChart(b);
    drawWeather(b);
    drawStations(b);
    drawFailures(b);
    b.flush();

    //Header and clock card text
    Ui::drawText(titleFont, french ? L"Poste instructeur" : L"Instructor station", irr::core::rect<irr::f32>(96, 26, pill.UpperLeftCorner.X - 20, 56), Ui::text);
    Ui::drawText(smallFont, exercise, irr::core::rect<irr::f32>(96, 58, pill.UpperLeftCorner.X - 20, 80), Ui::textDim);
    Ui::drawText(smallFont, state, irr::core::rect<irr::f32>(pill.UpperLeftCorner.X + 30, pill.UpperLeftCorner.Y, pill.LowerRightCorner.X - 12, pill.LowerRightCorner.Y), stateCol);
    const irr::f32 cx0 = clockCard.UpperLeftCorner.X + 24, cx1 = clockCard.LowerRightCorner.X - 24, cy = clockCard.UpperLeftCorner.Y;
    Ui::drawText(textFont, french ? L"Heure de l'exercice" : L"Exercise time", irr::core::rect<irr::f32>(cx0, cy + 14, cx1, cy + 40), Ui::textDim);
    Ui::drawText(bigFont, clock, irr::core::rect<irr::f32>(cx0, cy + 42, cx1, cy + 92), Ui::text);
    wchar_t speed[64];
    swprintf(speed, 64, french ? L"%ls   \u00B7   acc\u00E9l\u00E9ration x%g" : L"%ls   \u00B7   time x%g", date.c_str(), accelerator);
    Ui::drawText(smallFont, speed, irr::core::rect<irr::f32>(cx0, cy + 96, cx1, cy + 120), Ui::textDim);

    //All the labels of the panels
    for (size_t i = 0; i < labels.size(); i++) {
        const Label& l = labels[i];
        const irr::core::rect<irr::s32> clipRect = Ui::toI(l.r);
        Ui::drawText(l.font, l.text, l.r, l.col, (Ui::Align)l.align, l.clip ? &clipRect : 0);
    }

    //Stations typed in but not answering when the exercise started, and the footer
    std::wstring footer;
    if (!unreached.empty()) {
        footer = french ? L"Postes non joints : " : L"Stations not reached: ";
        for (size_t i = 0; i < unreached.size(); i++) { footer += (i ? L", " : L"") + unreached[i]; }
        footer += L"     \u00B7     ";
    }
    footer += (french ? L"Amarres actives : " : L"Active mooring lines: ") + std::to_wstring(lines);
    footer += french ? L"     \u00B7     Clavier : 0 = pause, Entr\u00E9e = reprendre     \u00B7     Carte : molette = zoom, glisser = d\u00E9placer, clic sur un navire = le choisir"
                     : L"     \u00B7     Keys: 0 = pause, Enter = resume     \u00B7     Chart: wheel = zoom, drag = move, click a ship = select it";
    Ui::drawText(smallFont, footer, irr::core::rect<irr::f32>(24, H - 34, W - 24, H - 10), unreached.empty() ? Ui::textFaint : irr::video::SColor(255, 255, 150, 130));

    IGUIElement::draw(); //buttons and the message box
}

void InstructorStation::drawChart(irr::gui::PanelBatch& b)
{
    const irr::f32 x0 = chartCard.UpperLeftCorner.X + 20, y0 = chartCard.UpperLeftCorner.Y;
    label(textFont, french ? L"Carte en direct" : L"Live chart", irr::core::rect<irr::f32>(x0, y0 + 10, x0 + 300, y0 + 40), Ui::text);
    const irr::f32 cr = chartCard.LowerRightCorner.X - 20;
    chip(b, irr::core::rect<irr::f32>(cr - 100, y0 + 12, cr, y0 + 38), french ? L"Fond" : L"Style", false, Z_CHART_STYLE, 0);
    chip(b, irr::core::rect<irr::f32>(cr - 230, y0 + 12, cr - 110, y0 + 38), french ? L"Voir les navires" : L"Fit ships", false, Z_CHART_FIT, 0);
    if (!chartLoaded) {
        label(textFont, french ? L"Carte de la zone indisponible" : L"No chart for this area", chartArea, Ui::textDim, 1);
    }

    //Tracks, then the ships over them, clipped to the chart
    const irr::core::rect<irr::f32> clip = chartArea;
    auto inside = [&](const irr::core::vector2df& p) { return clip.isPointInside(p); };
    auto toScreen = [&](irr::f32 x, irr::f32 z) {
        if (!chartLoaded) { return irr::core::vector2df(chartArea.getCenter().X, chartArea.getCenter().Y); }
        const irr::core::position2di s = chart->toScreenXZ(x, z);
        return irr::core::vector2df((irr::f32)s.X, (irr::f32)s.Y);
    };
    for (size_t i = 0; i < stations.size() && i < tracks.size(); i++) {
        const irr::video::SColor col = stationColour(i);
        const std::deque<irr::core::vector2df>& t = tracks[i];
        for (size_t k = 1; k < t.size(); k++) {
            const irr::core::vector2df a = toScreen(t[k - 1].X, t[k - 1].Y), c = toScreen(t[k].X, t[k].Y);
            if (!inside(a) || !inside(c)) { continue; }
            b.line(a, c, 2.0f, Ui::alpha(col, 70 + (irr::u32)(150 * k / t.size())));
        }
    }
    for (size_t i = 0; i < stations.size(); i++) {
        const Station& s = stations[i];
        if (!s.reported) { continue; }
        const irr::core::vector2df p = toScreen(s.x, s.z);
        if (!inside(p)) { continue; }
        const irr::video::SColor col = stationColour(i);
        //Someone has an alarm not acknowledged: a red halo
        if (s.unacked > 0) { b.disc(p, 22, Ui::alpha(Ui::danger, 120), Ui::alpha(Ui::danger, 0)); }
        if ((int)i == selected) { b.sector(p, 14, 17, 0, 360, Ui::text, Ui::text, false); }
        const irr::f32 a = s.heading * irr::core::DEGTORAD;
        const irr::core::vector2df fwd(std::sin(a), -std::cos(a)), side(std::cos(a), std::sin(a));
        b.tri(p + fwd * 13.0f, p - fwd * 8.0f + side * 7.0f, p - fwd * 8.0f - side * 7.0f, col);
        //Heading line: where the ship goes in the next six minutes
        if (chartLoaded && s.speedKts > 0.2f) {
            const irr::f32 dist = s.speedKts * 1852.0f / 10.0f;
            const irr::core::vector2df q = toScreen(s.x + dist * std::sin(a), s.z + dist * std::cos(a));
            if (inside(q)) { b.line(p, q, 1.5f, Ui::alpha(col, 200)); }
        }
        std::wstring name = std::to_wstring(i + 1) + L"  " + (s.ship.empty() ? s.host : s.ship);
        label(smallFont, name, irr::core::rect<irr::f32>(p.X + 16, p.Y - 20, p.X + 300, p.Y - 2), col, 0, false);
    }

    //Scale bar
    if (chartLoaded) {
        double ax, az, bx, bz;
        chart->toXZ(irr::core::position2di(0, 0), ax, az);
        chart->toXZ(irr::core::position2di(100, 0), bx, bz);
        const double mpp = std::fabs(bx - ax) / 100.0;
        if (mpp > 0) {
            const double nice[] = { 50, 100, 200, 500, 926, 1852, 3704, 9260, 18520, 37040 };
            double len = nice[0];
            for (int k = 0; k < 10; k++) { if (nice[k] / mpp <= 160) { len = nice[k]; } }
            const irr::f32 px = (irr::f32)(len / mpp);
            const irr::f32 sx = chartArea.UpperLeftCorner.X + 16, sy = chartArea.LowerRightCorner.Y - 18;
            b.rect(irr::core::rect<irr::f32>(sx - 6, sy - 22, sx + px + 70, sy + 8), irr::video::SColor(170, 8, 16, 30));
            b.rect(irr::core::rect<irr::f32>(sx, sy - 2, sx + px, sy + 1), Ui::text);
            b.rect(irr::core::rect<irr::f32>(sx, sy - 7, sx + 2, sy + 1), Ui::text);
            b.rect(irr::core::rect<irr::f32>(sx + px - 2, sy - 7, sx + px, sy + 1), Ui::text);
            wchar_t t[32];
            if (len >= 926) { swprintf(t, 32, L"%g NM", len / 1852.0); } else { swprintf(t, 32, L"%g m", len); }
            label(smallFont, t, irr::core::rect<irr::f32>(sx + px + 8, sy - 20, sx + px + 70, sy + 6), Ui::text);
        }
    }
}

void InstructorStation::drawWeather(irr::gui::PanelBatch& b)
{
    const irr::f32 x0 = weatherCard.UpperLeftCorner.X + 20, x1 = weatherCard.LowerRightCorner.X - 20, y0 = weatherCard.UpperLeftCorner.Y;
    label(textFont, french ? L"M\u00E9t\u00E9o  \u00B7  tous les postes" : L"Weather  \u00B7  all stations", irr::core::rect<irr::f32>(x0, y0 + 10, x1, y0 + 38), Ui::text);
    label(smallFont, weatherSent.empty() ? (french ? L"Les stagiaires ont la m\u00E9t\u00E9o du sc\u00E9nario." : L"The students have the scenario's weather.") : weatherSent,
        irr::core::rect<irr::f32>(x0, y0 + 38, x1, y0 + 58), weatherSent.empty() ? Ui::textFaint : Ui::accentHi, 0, true);

    //Presets: the scenario's, then the weather window's, two columns
    const irr::f32 colW = (x1 - x0 - 8) / 2, chipH = 26;
    irr::f32 y = y0 + 66;
    for (int i = -1; i < (int)presets.size(); i++) {
        const int k = i + 1;
        const irr::f32 cx = x0 + (k % 2) * (colW + 8), cy = y + (k / 2) * 31;
        chip(b, irr::core::rect<irr::f32>(cx, cy, cx + colW, cy + chipH), i < 0 ? std::wstring(french ? L"Sc\u00E9nario" : L"Scenario") : presets[i].name,
            weatherPreset == i, Z_PRESET, i);
    }
    y += ((int)presets.size() + 2) / 2 * 31 + 6;

    auto row = [&](const std::wstring& title, const std::vector<std::wstring>& names, int chosen, ZoneKind kind) {
        label(smallFont, title, irr::core::rect<irr::f32>(x0, y, x1, y + 18), Ui::textFaint);
        y += 20;
        const irr::f32 w = (x1 - x0 - 4.0f * (names.size() - 1)) / names.size();
        for (size_t i = 0; i < names.size(); i++) {
            const irr::f32 cx = x0 + i * (w + 4);
            chip(b, irr::core::rect<irr::f32>(cx, y, cx + w, y + chipH), names[i], (int)i == chosen, kind, (int)i);
        }
        y += chipH + 10;
    };
    row(french ? L"VENT DU" : L"WIND FROM", french ? std::vector<std::wstring>{ L"N", L"NE", L"E", L"SE", L"S", L"SO", L"O", L"NO" }
        : std::vector<std::wstring>{ L"N", L"NE", L"E", L"SE", L"S", L"SW", L"W", L"NW" }, windDirIndex, Z_WINDDIR);
    row(french ? L"CHANGEMENT" : L"CHANGE", french ? std::vector<std::wstring>{ L"Imm\u00E9diat", L"1 min", L"5 min", L"15 min" }
        : std::vector<std::wstring>{ L"Now", L"1 min", L"5 min", L"15 min" }, transitionIndex, Z_TRANSITION);
    row(french ? L"\u00C9CLAIRAGE" : L"LIGHTING", french ? std::vector<std::wstring>{ L"Garder", L"Aube", L"Jour", L"Soir", L"Nuit" }
        : std::vector<std::wstring>{ L"As is", L"Dawn", L"Day", L"Dusk", L"Night" }, hourIndex, Z_HOUR);

    const irr::f32 by = std::min(y + 4, weatherCard.LowerRightCorner.Y - 50);
    const irr::f32 half = (x1 - x0 - 8) / 2;
    chip(b, irr::core::rect<irr::f32>(x0, by, x0 + half + 40, by + 34), french ? L"ENVOYER \u00C0 TOUS" : L"SEND TO ALL", true, Z_SEND_WEATHER, 0);
    chip(b, irr::core::rect<irr::f32>(x0 + half + 48, by, x1, by + 34), french ? L"Rendre la main" : L"Release", false, Z_RELEASE_WEATHER, 0);
}

void InstructorStation::drawStations(irr::gui::PanelBatch& b)
{
    const irr::f32 x0 = stationCard.UpperLeftCorner.X + 16, x1 = stationCard.LowerRightCorner.X - 16, y0 = stationCard.UpperLeftCorner.Y;
    label(textFont, french ? L"Postes des stagiaires" : L"Student stations", irr::core::rect<irr::f32>(x0 + 4, y0 + 10, x1, y0 + 38), Ui::text);
    label(smallFont, std::to_wstring(stations.size()), irr::core::rect<irr::f32>(x0, y0 + 10, x1 - 4, y0 + 38), Ui::textDim, 2);

    irr::f32 y = y0 + 46;
    //"All": the target of the failures and messages
    {
        const irr::core::rect<irr::f32> r(x0, y, x1, y + 30);
        const bool on = selected < 0;
        Ui::roundRect(b, r, 8, on ? Ui::alpha(Ui::accent, 90) : irr::video::SColor(255, 16, 30, 52), on ? Ui::alpha(Ui::accent, 70) : irr::video::SColor(255, 14, 27, 47));
        label(textFont, french ? L"Tous les postes" : L"All stations", irr::core::rect<irr::f32>(x0 + 12, y, x1, y + 30), on ? Ui::text : Ui::textDim);
        Zone z; z.r = r; z.kind = Z_STATION; z.value = -1; zones.push_back(z);
        y += 36;
    }
    for (size_t i = 0; i < stations.size(); i++) {
        const irr::core::rect<irr::f32> r(x0, y, x1, y + 44);
        if (r.LowerRightCorner.Y > stationCard.LowerRightCorner.Y - 6) { break; }
        const Station& s = stations[i];
        const bool on = (int)i == selected;
        Ui::roundRect(b, r, 8, on ? Ui::alpha(Ui::accent, 90) : irr::video::SColor(255, 16, 30, 52), on ? Ui::alpha(Ui::accent, 70) : irr::video::SColor(255, 14, 27, 47));
        b.rect(irr::core::rect<irr::f32>(x0, y + 6, x0 + 4, y + 38), stationColour(i));
        if (s.unacked > 0 && ((device->getTimer()->getRealTime() / 500) % 2 == 0)) {
            Ui::roundRectOutline(b, r, 8, 2.0f, Ui::danger);
        }
        const std::wstring name = std::to_wstring(i + 1) + L"  " + (s.ship.empty() ? s.host : s.ship) + (s.ship.empty() ? L"" : L"   (" + s.host + L")");
        label(textFont, name, irr::core::rect<irr::f32>(x0 + 12, y + 2, x1 - 120, y + 22), Ui::text, 0, true);
        wchar_t mv[48];
        swprintf(mv, 48, L"%.1f nd  %03.0f\u00B0", s.speedKts, s.heading);
        label(smallFont, s.reported ? std::wstring(mv) : std::wstring(french ? L"pas encore de position" : L"no position yet"),
            irr::core::rect<irr::f32>(x1 - 130, y + 2, x1 - 8, y + 22), Ui::textDim, 2);

        //Second line: what needs the instructor's eye
        std::wstring st;
        irr::video::SColor stCol = Ui::textFaint;
        if (!s.hasStatus) {
            st = french ? L"(simulateur sans retour d'\u00E9tat)" : L"(simulator does not report its state)";
        }
        else {
            wchar_t buf[64];
            if (s.unacked > 0) {
                swprintf(buf, 64, french ? L"%d alarme(s) non acquitt\u00E9e(s) depuis %.0f s" : L"%d alarm(s) not acknowledged for %.0f s", s.unacked, s.oldestUnacked);
                st += buf;
                stCol = Ui::danger;
            }
            int failures = (s.pump1 ? 0 : 1) + (s.pump2 ? 0 : 1) + (s.followUp ? 0 : 1);
            for (int f = 0; f < 5; f++) { if (s.failure[f]) { failures++; } }
            if (failures) { swprintf(buf, 64, french ? L"%d avarie(s)" : L"%d failure(s)", failures); st += (st.empty() ? L"" : L"  \u00B7  ") + std::wstring(buf); }
            if (s.scheduled) { swprintf(buf, 64, french ? L"%d programm\u00E9e(s)" : L"%d scheduled", s.scheduled); st += (st.empty() ? L"" : L"  \u00B7  ") + std::wstring(buf); }
            if (s.collisions) { swprintf(buf, 64, french ? L"abordage %d" : L"collision %d", s.collisions); st += (st.empty() ? L"" : L"  \u00B7  ") + std::wstring(buf); stCol = Ui::danger; }
            if (s.groundings) { swprintf(buf, 64, french ? L"\u00E9chouement %d" : L"grounding %d", s.groundings); st += (st.empty() ? L"" : L"  \u00B7  ") + std::wstring(buf); stCol = Ui::danger; }
            swprintf(buf, 64, french ? L"fond %.1f m" : L"depth %.1f m", s.depth);
            st += (st.empty() ? L"" : L"  \u00B7  ") + std::wstring(buf);
            if (s.depthAlarm && stCol != Ui::danger) { stCol = Ui::warning; }
            if (stCol == Ui::textFaint && failures) { stCol = Ui::warning; }
        }
        label(smallFont, st, irr::core::rect<irr::f32>(x0 + 12, y + 22, x1 - 8, y + 42), stCol, 0, true);
        Zone z; z.r = r; z.kind = Z_STATION; z.value = (int)i; zones.push_back(z);
        y += 48;
    }
}

void InstructorStation::drawFailures(irr::gui::PanelBatch& b)
{
    const irr::f32 x0 = failureCard.UpperLeftCorner.X + 20, x1 = failureCard.LowerRightCorner.X - 20, y0 = failureCard.UpperLeftCorner.Y;
    label(textFont, (french ? L"Avaries  \u00B7  " : L"Failures  \u00B7  ") + targetName(), irr::core::rect<irr::f32>(x0, y0 + 10, x1, y0 + 38), Ui::text, 0, true);

    const Station* s = (selected >= 0 && selected < (int)stations.size() && stations[selected].hasStatus) ? &stations[selected] : 0;
    irr::f32 y = y0 + 44;
    const irr::f32 chipH = 26;
    {
        label(smallFont, french ? L"D\u00C9CLENCHEMENT" : L"WHEN", irr::core::rect<irr::f32>(x0, y, x1, y + 18), Ui::textFaint);
        y += 20;
        const std::vector<std::wstring> names = french ? std::vector<std::wstring>{ L"Imm\u00E9diat", L"1 min", L"3 min", L"5 min", L"Au hasard" }
            : std::vector<std::wstring>{ L"Now", L"1 min", L"3 min", L"5 min", L"Random" };
        const irr::f32 w = (x1 - x0 - 16) / 5;
        for (int i = 0; i < 5; i++) {
            chip(b, irr::core::rect<irr::f32>(x0 + i * (w + 4), y, x0 + i * (w + 4) + w, y + chipH), names[i], i == delayIndex, Z_DELAY, i);
        }
        y += chipH + 12;
    }

    //One row per item: its name, then a chip per state. The state shown is the selected student's.
    const irr::f32 labelW = 118;
    auto item = [&](const std::wstring& name, int action, const std::vector<std::wstring>& states, const std::vector<int>& levels, int current) {
        label(smallFont, name, irr::core::rect<irr::f32>(x0, y, x0 + labelW, y + chipH), Ui::textDim);
        const irr::f32 w = (x1 - x0 - labelW - 4.0f * (states.size() - 1)) / states.size();
        for (size_t i = 0; i < states.size(); i++) {
            const irr::f32 cx = x0 + labelW + i * (w + 4);
            const int level = levels[i];
            chip(b, irr::core::rect<irr::f32>(cx, y, cx + w, y + chipH), states[i], s != 0 && current == level, Z_FAILURE, action * 10 + level,
                level == 2 ? 1 : (level == 1 ? 2 : 0));
        }
        y += chipH + 6;
    };
    const std::wstring ok = french ? L"Service" : L"Working";
    const std::wstring failed = french ? L"Panne" : L"Failed";
    item(french ? L"Pompe de barre 1" : L"Steering pump 1", 100, { ok, failed }, { 0, 2 }, s ? (s->pump1 ? 0 : 2) : -1);
    item(french ? L"Pompe de barre 2" : L"Steering pump 2", 101, { ok, failed }, { 0, 2 }, s ? (s->pump2 ? 0 : 2) : -1);
    item(french ? L"Barre asservie" : L"Follow-up", 102, { ok, failed }, { 0, 2 }, s ? (s->followUp ? 0 : 2) : -1);
    item(french ? L"Machine b\u00E2bord" : L"Port engine", 0, { ok, french ? L"R\u00E9duite" : L"Reduced", failed }, { 0, 1, 2 }, s ? s->failure[0] : -1);
    item(french ? L"Machine tribord" : L"Stbd engine", 1, { ok, french ? L"R\u00E9duite" : L"Reduced", failed }, { 0, 1, 2 }, s ? s->failure[1] : -1);
    item(french ? L"Gyrocompas" : L"Gyro", 2, { ok, french ? L"D\u00E9rive" : L"Drift", failed }, { 0, 1, 2 }, s ? s->failure[2] : -1);
    item(L"GPS", 3, { ok, french ? L"D\u00E9rive" : L"Drift", french ? L"Perte" : L"Lost" }, { 0, 1, 2 }, s ? s->failure[3] : -1);
    item(L"Radar", 4, { ok, failed }, { 0, 2 }, s ? s->failure[4] : -1);

    if (s && (s->failure[2] == 1 || s->failure[3] == 1)) {
        wchar_t buf[96];
        swprintf(buf, 96, french ? L"Erreur gyro %+.1f\u00B0   \u00B7   \u00E9cart GPS %.0f m" : L"Gyro error %+.1f\u00B0   \u00B7   GPS offset %.0f m", s->gyroError, s->gpsError);
        label(smallFont, buf, irr::core::rect<irr::f32>(x0, y, x1, y + 18), Ui::warning);
    }
    y += 22;
    const irr::f32 half = (x1 - x0 - 8) / 2;
    chip(b, irr::core::rect<irr::f32>(x0, y, x0 + half, y + 32), french ? L"Tout r\u00E9parer" : L"Repair all", false, Z_REPAIR, 0);
    chip(b, irr::core::rect<irr::f32>(x0 + half + 8, y, x1, y + 32), french ? L"Homme \u00E0 la mer !" : L"Man overboard!", false, Z_MOB, 0);
    y += 42;
    label(smallFont, (french ? L"MESSAGE  \u00B7  " : L"MESSAGE  \u00B7  ") + targetName(), irr::core::rect<irr::f32>(x0, y, x1, y + 18), Ui::textFaint, 0, true);
    y += 22;
    if (messageBox) {
        messageBox->setRelativePosition(Ui::toI(irr::core::rect<irr::f32>(x0, y, x1 - 100, y + 30)));
    }
    chip(b, irr::core::rect<irr::f32>(x1 - 92, y, x1, y + 30), french ? L"Envoyer" : L"Send", true, Z_SEND_MESSAGE, 0);
}

//-------------------------------------------------------------------------------------------------
//Input
//-------------------------------------------------------------------------------------------------

int InstructorStation::stationAt(irr::core::position2di p) const
{
    if (!chartLoaded) { return -1; }
    int best = -1;
    irr::f32 bestD = 18.0f * 18.0f;
    for (size_t i = 0; i < stations.size(); i++) {
        if (!stations[i].reported) { continue; }
        const irr::core::position2di s = chart->toScreenXZ(stations[i].x, stations[i].z);
        const irr::f32 dx = (irr::f32)(s.X - p.X), dy = (irr::f32)(s.Y - p.Y);
        if (dx * dx + dy * dy < bestD) { bestD = dx * dx + dy * dy; best = (int)i; }
    }
    return best;
}

bool InstructorStation::OnEvent(const irr::SEvent& event)
{
    if (event.EventType == irr::EET_GUI_EVENT && event.GUIEvent.Caller == messageBox && event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_ENTER) {
        sendMessage();
        return true;
    }
    if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
        const irr::SEvent::SMouseInput& m = event.MouseInput;
        mouse = irr::core::position2di(m.X, m.Y);
        const bool overChart = chartArea.isPointInside(irr::core::vector2df((irr::f32)m.X, (irr::f32)m.Y));
        switch (m.Event) {
        case irr::EMIE_MOUSE_WHEEL:
            if (overChart && chartLoaded) {
                chart->zoomAt(mouse, m.Wheel > 0 ? 0.8f : 1.25f);
                chartTouched = true;
                return true;
            }
            break;
        case irr::EMIE_LMOUSE_PRESSED_DOWN:
            for (size_t i = 0; i < zones.size(); i++) {
                if (zones[i].r.isPointInside(irr::core::vector2df((irr::f32)m.X, (irr::f32)m.Y))) {
                    click(zones[i]);
                    return true;
                }
            }
            if (overChart) {
                dragging = true;
                dragMoved = false;
                dragStart = dragLast = mouse;
                Environment->setFocus(this);
                return true;
            }
            break;
        case irr::EMIE_MOUSE_MOVED:
            if (dragging && chartLoaded) {
                if (std::abs(mouse.X - dragStart.X) + std::abs(mouse.Y - dragStart.Y) > 4) { dragMoved = true; }
                if (dragMoved) { chart->panPixels(mouse.X - dragLast.X, mouse.Y - dragLast.Y); chartTouched = true; }
                dragLast = mouse;
                return true;
            }
            break;
        case irr::EMIE_LMOUSE_LEFT_UP:
            if (dragging) {
                dragging = false;
                if (!dragMoved) {
                    const int s = stationAt(mouse);
                    if (s >= 0) { selected = s; }
                }
                return true;
            }
            break;
        default:
            break;
        }
    }
    return IGUIElement::OnEvent(event);
}

void InstructorStation::click(const Zone& z)
{
    switch (z.kind) {
    case Z_PRESET: weatherPreset = z.value; break;
    case Z_WINDDIR: windDirIndex = z.value; break;
    case Z_TRANSITION: transitionIndex = z.value; break;
    case Z_HOUR: hourIndex = z.value; break;
    case Z_DELAY: delayIndex = z.value; break;
    case Z_STATION: selected = z.value; break;
    case Z_CHART_FIT: fitChartToShips(); break;
    case Z_CHART_STYLE: if (chartLoaded) { chart->nextStyle(); } break;
    case Z_SEND_WEATHER: {
        //The chosen preset (or the scenario's weather), with the chosen wind direction
        irr::f32 cloud, windKn, variation, gust, visibility, rain, snow = 0, dust = 0, sea;
        int significant;
        std::wstring name;
        if (weatherPreset >= 0 && weatherPreset < (int)presets.size()) {
            const Preset& p = presets[weatherPreset];
            cloud = p.cloud; windKn = p.windKn; variation = p.windVariation; gust = p.gustKn; visibility = p.visibilityNm;
            rain = p.rain; snow = p.snow; dust = p.dust; sea = p.sea; significant = p.significant; name = p.name;
        }
        else {
            cloud = scenarioWeather.rain > 0 ? 0.85f : 0.3f; windKn = scenarioWeather.windKn; variation = 15; gust = scenarioWeather.windKn * 0.3f;
            visibility = scenarioWeather.visibilityNm > 0 ? scenarioWeather.visibilityNm : 10; rain = scenarioWeather.rain; sea = scenarioWeather.sea;
            significant = 0; name = french ? L"m\u00E9t\u00E9o du sc\u00E9nario" : L"scenario weather";
        }
        const irr::f32 transitions[4] = { 0, 60, 300, 900 };
        const irr::f32 hours[5] = { -1, 6.5f, 12, 19.5f, 23 };
        const irr::f32 windDir = windDirIndex * 45.0f;
        std::string c = "ICW," + num(cloud) + "," + num(windKn) + "," + num(windDir) + "," + num(variation) + "," + num(gust) + "," + num(visibility) + ","
            + num(rain) + "," + num(snow) + "," + num(dust) + "," + num(sea) + "," + std::to_string(significant) + "," + num(transitions[transitionIndex]) + ","
            + "-1,0,0," + num(hours[hourIndex]) + ",1";
        Command cmd; cmd.station = -1; cmd.text = c;
        commands.push_back(cmd);
        static const wchar_t* dirs[8] = { L"N", L"NE", L"E", L"SE", L"S", L"SO", L"O", L"NO" };
        wchar_t buf[160];
        swprintf(buf, 160, french ? L"Envoy\u00E9e \u00E0 %ls : %ls, vent %.0f nd de %ls" : L"Sent at %ls: %ls, wind %.0f kn from %ls",
            clock.c_str(), name.c_str(), windKn, dirs[windDirIndex]);
        weatherSent = buf;
        break;
    }
    case Z_RELEASE_WEATHER: {
        Command cmd; cmd.station = -1; cmd.text = "ICL,0";
        commands.push_back(cmd);
        weatherSent = french ? L"M\u00E9t\u00E9o rendue aux stagiaires (\u00E0 " + clock + L")" : L"Weather released to the students (at " + clock + L")";
        break;
    }
    case Z_FAILURE: {
        const int action = z.value / 10, level = z.value % 10;
        const irr::f32 delays[4] = { 0, 60, 180, 300 };
        for (size_t i = 0; i < stations.size(); i++) {
            if (selected >= 0 && (int)i != selected) { continue; }
            //At random: each student at his own moment, between 2 and 10 minutes
            const irr::f32 delay = level == 0 ? 0.0f : (delayIndex >= 4 ? 120.0f + 480.0f * (std::rand() / (irr::f32)RAND_MAX) : delays[delayIndex]);
            Command cmd; cmd.station = (int)i;
            cmd.text = "ICF," + std::to_string(action) + "," + std::to_string(level) + "," + num(delay);
            commands.push_back(cmd);
        }
        break;
    }
    case Z_REPAIR: sendToTargets("ICR"); break;
    case Z_MOB: sendToTargets("ICO"); break;
    case Z_SEND_MESSAGE: sendMessage(); break;
    }
}

void InstructorStation::sendToTargets(const std::string& text)
{
    for (size_t i = 0; i < stations.size(); i++) {
        if (selected >= 0 && (int)i != selected) { continue; }
        Command cmd; cmd.station = (int)i; cmd.text = text;
        commands.push_back(cmd);
    }
}

void InstructorStation::sendMessage()
{
    if (!messageBox) { return; }
    const std::wstring text = messageBox->getText();
    if (text.empty()) { return; }
    //Each character as four hex digits: any accent goes through, and commas do not split it
    std::string hex = "ICT,";
    for (size_t i = 0; i < text.size(); i++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%04X", (unsigned int)(text[i] & 0xFFFF));
        hex += buf;
    }
    sendToTargets(hex);
    messageBox->setText(L"");
}
