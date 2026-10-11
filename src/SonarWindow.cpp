/*   NAUTITECH - Simulateur de Navigation
     The sonar station window.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "SonarWindow.hpp"
#include "ConsoleWindow.hpp"
#include "SimulationModel.hpp"
#include "IniFile.hpp"
#include "Utilities.hpp"
#include "launcher/HudText.hpp"
#include <cmath>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <algorithm>

namespace {
    //Colours of the station
    const irr::video::SColor BG(255, 6, 11, 20);
    const irr::video::SColor HEADER(255, 12, 21, 36);
    const irr::video::SColor CARD(255, 15, 26, 44);
    const irr::video::SColor CARD_LINE(255, 34, 52, 78);
    const irr::video::SColor KEY(255, 24, 40, 64);
    const irr::video::SColor KEY_HOVER(255, 34, 56, 88);
    const irr::video::SColor KEY_ON(255, 40, 120, 210);
    const irr::video::SColor TEXT(255, 214, 226, 240);
    const irr::video::SColor DIM(255, 128, 148, 172);
    const irr::video::SColor ACCENT(255, 90, 180, 255);
    const irr::video::SColor RING(150, 150, 190, 220);
    const irr::video::SColor SWEEP(220, 120, 255, 170);
    const irr::video::SColor MARK(255, 255, 230, 90);
    const irr::video::SColor PICTURE_BG(255, 4, 10, 40);

    std::wstring fmt(const wchar_t* format, double v)
    {
        wchar_t buf[64];
        swprintf(buf, 64, format, v);
        return buf;
    }

    std::wstring metres(irr::f32 m)
    {
        return (m >= 1000.0f && std::fmod(m, 1000.0f) == 0.0f) ? fmt(L"%.0f km", m / 1000.0f)
            : ((m >= 1000.0f) ? fmt(L"%.1f km", m / 1000.0f) : fmt(L"%.0f m", m));
    }

    //Fish finder colours, weak to strong: dark blue, light blue, green, yellow, orange, red, brown
    void buildPalette(std::vector<irr::u32>& p)
    {
        struct Stop { int v; int r, g, b; };
        const Stop stops[] = {
            { 0, 4, 10, 40 }, { 30, 10, 28, 110 }, { 60, 20, 90, 200 }, { 90, 30, 170, 225 }, { 115, 30, 200, 150 },
            { 140, 60, 215, 60 }, { 165, 190, 225, 40 }, { 190, 250, 205, 25 }, { 210, 250, 140, 15 },
            { 230, 235, 50, 25 }, { 245, 170, 20, 30 }, { 255, 120, 25, 60 }
        };
        const int n = sizeof(stops) / sizeof(stops[0]);
        p.resize(256);
        for (int v = 0; v < 256; v++) {
            int i = 0;
            while (i < n - 2 && v > stops[i + 1].v) { i++; }
            const Stop& a = stops[i];
            const Stop& b = stops[i + 1];
            const irr::f32 t = (b.v == a.v) ? 0.0f : irr::core::clamp((irr::f32)(v - a.v) / (b.v - a.v), 0.0f, 1.0f);
            const int r = (int)(a.r + (b.r - a.r) * t), g = (int)(a.g + (b.g - a.g) * t), bl = (int)(a.b + (b.b - a.b) * t);
            p[v] = 0xFF000000u | ((irr::u32)r << 16) | ((irr::u32)g << 8) | (irr::u32)bl;
        }
    }

    irr::core::rect<irr::s32> toInt(const irr::core::rect<irr::f32>& r)
    {
        return irr::core::rect<irr::s32>((irr::s32)r.UpperLeftCorner.X, (irr::s32)r.UpperLeftCorner.Y,
            (irr::s32)r.LowerRightCorner.X, (irr::s32)r.LowerRightCorner.Y);
    }

    //A step for the scale marks: 1, 2 or 5 times a power of ten
    irr::f32 niceStep(irr::f32 span, int marks)
    {
        const irr::f32 raw = span / irr::core::max_(marks, 1);
        const irr::f32 p = std::pow(10.0f, std::floor(std::log10(irr::core::max_(raw, 0.001f))));
        const irr::f32 f = raw / p;
        return p * ((f < 1.5f) ? 1.0f : ((f < 3.5f) ? 2.0f : ((f < 7.5f) ? 5.0f : 10.0f)));
    }
}

SonarWindow::SonarWindow(irr::IrrlichtDevice* device, SimulationModel* model, irr::u32 instance)
    : device(device), driver(device->getVideoDriver()), model(model), window(0), windowUsed(false), instance(instance),
      placeX(160), placeY(120), placeW(0), placeH(0), placeFullScreen(false), onScreen(false),
      mode(MODE_BOTH), headUp(true), markerOn(false), markerBearing(0), markerRange(0), mouseInside(false),
      lastRealMs(0), lastRenderMs(0), frameMs(33), fullScreenRequest(false), renderCount(0), rateStartMs(0), rate(0), k(1.0f), radius360(0),
      tex360(0), texEcho(0), serial360Drawn(0xFFFFFFFF), serialEchoDrawn(0xFFFFFFFF), headingDrawn(-1000.0f), lastTex360Ms(0),
      fontScale(0), fontTitle(0), fontText(0), fontSmall(0), fontBig(0)
{
    sonar.setModel(model);
    buildPalette(palette);
    placementFile = Utilities::getUserDir() + ((instance > 1) ? "sonarWindow-" + std::to_string(instance) + ".ini" : std::string("sonarWindow.ini"));
    placeX = IniFile::iniFileTos32(placementFile, "X", placeX);
    placeY = IniFile::iniFileTos32(placementFile, "Y", placeY);
    placeW = IniFile::iniFileTou32(placementFile, "Width");
    placeH = IniFile::iniFileTou32(placementFile, "Height");
    placeFullScreen = IniFile::iniFileTou32(placementFile, "FullScreen") == 1;
    mode = (Mode)irr::core::clamp((int)IniFile::iniFileTou32(placementFile, "Mode", MODE_BOTH), 0, 2);
    headUp = IniFile::iniFileTou32(placementFile, "HeadUp", 1) == 1;
    sonar.setRangeIndex360((int)IniFile::iniFileTou32(placementFile, "Range360", 3));
    sonar.setTilt(IniFile::iniFileTof32(placementFile, "Tilt", 10.0f));
    sonar.setGain360((int)IniFile::iniFileTou32(placementFile, "Gain360", 5));
    sonar.setSounderGain((int)IniFile::iniFileTou32(placementFile, "GainSounder", 5));
    sonar.setLowFrequency(IniFile::iniFileTou32(placementFile, "Frequency", 50) == 50);

    //For every pixel of the 360 picture: its angle and its range bin
    lutAngle.resize(TEX * TEX);
    lutBin.resize(TEX * TEX);
    const irr::f32 c = TEX * 0.5f;
    for (int y = 0; y < TEX; y++) {
        for (int x = 0; x < TEX; x++) {
            const irr::f32 dx = x + 0.5f - c, dy = y + 0.5f - c;
            const irr::f32 r = std::sqrt(dx * dx + dy * dy) / c;
            const size_t i = (size_t)y * TEX + x;
            if (r >= 1.0f) { lutAngle[i] = 0xFFFF; lutBin[i] = 0; continue; }
            irr::f32 a = std::atan2(dx, -dy) * irr::core::RADTODEG;
            if (a < 0) { a += 360.0f; }
            lutAngle[i] = (irr::u16)irr::core::clamp((int)(a * 10.0f), 0, 3599);
            lutBin[i] = (irr::u16)irr::core::clamp((int)(r * Sonar::BINS), 0, Sonar::BINS - 1);
        }
    }
}

SonarWindow::~SonarWindow()
{
    shutdown();
}

void SonarWindow::shutdown()
{
    if (window) {
        if (window->isOpen()) { savePlacement(true); }
        window->close();
        delete window;
        window = 0;
    }
    freeFonts();
    if (tex360) { driver->removeTexture(tex360); tex360 = 0; }
    if (texEcho) { driver->removeTexture(texEcho); texEcho = 0; }
}

void SonarWindow::setScreen(const irr::core::rect<irr::s32>& area)
{
    screenArea = area;
    onScreen = true;
}

void SonarWindow::restore()
{
    if (onScreen || IniFile::iniFileTou32(placementFile, "Open") == 1) { open(); }
}

bool SonarWindow::isOpen() const
{
    return window && window->isOpen();
}

void SonarWindow::toggle()
{
    if (isOpen()) { close(); }
    else { open(); }
}

const irr::video::SExposedVideoData* SonarWindow::mainVideoData() const
{
    return (windowUsed && window) ? &window->mainVideoData() : 0;
}

void SonarWindow::open()
{
    if (isOpen()) { return; }
    if (!window) { window = new ConsoleWindow(); }
    irr::s32 x = placeX, y = placeY;
    irr::u32 w = placeW, h = placeH;
    if (onScreen) {
        x = screenArea.UpperLeftCorner.X;
        y = screenArea.UpperLeftCorner.Y;
        w = (irr::u32)screenArea.getWidth();
        h = (irr::u32)screenArea.getHeight();
    }
    else if (w < 400 || h < 300) {
        const irr::core::dimension2du main = driver->getScreenSize();
        w = irr::core::max_((irr::u32)(main.Width * 0.6f), 800u);
        h = irr::core::max_((irr::u32)(main.Height * 0.6f), 500u);
    }
    if (!window->open(device, L"NAUTITECH - Sonar", x, y, w, h, onScreen)) {
        std::cerr << "Could not open the sonar window (OpenGL driver needed)." << std::endl;
        return;
    }
    if (!onScreen && placeFullScreen) { window->setFullScreen(true); }
    windowUsed = true;
    lastRealMs = device->getTimer()->getRealTime();
    savePlacement(true);
}

void SonarWindow::close()
{
    if (!isOpen()) { return; }
    savePlacement(false);
    window->close();
}

void SonarWindow::savePlacement(bool isOpenNow)
{
    if (window && window->isOpen() && !onScreen) {
        window->getPlacement(placeX, placeY, placeW, placeH);
        placeFullScreen = window->isFullScreen();
    }
    std::ofstream f(placementFile.c_str());
    if (!f) { return; }
    f << "Open=" << (isOpenNow ? 1 : 0) << std::endl;
    f << "X=" << placeX << std::endl;
    f << "Y=" << placeY << std::endl;
    if (placeW > 0 && placeH > 0) {
        f << "Width=" << placeW << std::endl;
        f << "Height=" << placeH << std::endl;
    }
    f << "FullScreen=" << (placeFullScreen ? 1 : 0) << std::endl;
    f << "Mode=" << (int)mode << std::endl;
    f << "HeadUp=" << (headUp ? 1 : 0) << std::endl;
    f << "Range360=" << sonar.rangeIndex360() << std::endl;
    f << "Tilt=" << sonar.tilt() << std::endl;
    f << "Gain360=" << sonar.gain360() << std::endl;
    f << "GainSounder=" << sonar.sounderGain() << std::endl;
    f << "Frequency=" << (sonar.lowFrequency() ? 50 : 200) << std::endl;
}

void SonarWindow::loadFonts(irr::f32 scale)
{
    freeFonts();
    fontScale = scale;
    irr::gui::IGUIFont* fallback = device->getGUIEnvironment()->getSkin() ? device->getGUIEnvironment()->getSkin()->getFont() : 0;
    const std::string folder = "media/fonts/barlow-condensed/";
    fontTitle = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 28 * scale, fallback);
    fontText = new HudFont(driver, folder + "BarlowCondensed-Medium.ttf", 19 * scale, fallback);
    fontSmall = new HudFont(driver, folder + "BarlowCondensed-Regular.ttf", 15 * scale, fallback);
    fontBig = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 44 * scale, fallback);
}

void SonarWindow::freeFonts()
{
    delete fontTitle; fontTitle = 0;
    delete fontText; fontText = 0;
    delete fontSmall; fontSmall = 0;
    delete fontBig; fontBig = 0;
}

irr::video::ITexture* SonarWindow::makeTexture(const char* name)
{
    const bool mipMaps = driver->getTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS);
    const bool keep = driver->getTextureCreationFlag(irr::video::ETCF_ALLOW_MEMORY_COPY);
    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, false);
    driver->setTextureCreationFlag(irr::video::ETCF_ALLOW_MEMORY_COPY, true);
    irr::video::ITexture* t = driver->addTexture(irr::core::dimension2du(TEX, TEX), name, irr::video::ECF_A8R8G8B8);
    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, mipMaps);
    driver->setTextureCreationFlag(irr::video::ETCF_ALLOW_MEMORY_COPY, keep);
    return t;
}

//-------------------------------------------------------------------------------------------------
//Frame
//-------------------------------------------------------------------------------------------------
void SonarWindow::render()
{
    if (!isOpen()) { return; }
    const irr::u32 now = device->getTimer()->getRealTime();
    const irr::f32 dt = (now - lastRealMs) / 1000.0f;
    lastRealMs = now;

    if (fullScreenRequest) {
        fullScreenRequest = false;
        window->setFullScreen(!window->isFullScreen());
        savePlacement(true);
    }
    const irr::core::dimension2du size = window->getClientSize();
    layout(size.Width, size.Height);
    handleInput();
    if (!isOpen()) { return; }
    window->keepAboveSimulator();
    sonar.update(dt);

    if (now - rateStartMs >= 1000) {
        rate = renderCount * 1000 / (now - rateStartMs);
        renderCount = 0;
        rateStartMs = now;
    }
    if (now - lastRenderMs < frameMs) { return; }
    if (size.Width < 64 || size.Height < 64) { return; }   //minimised
    lastRenderMs = now;
    renderCount++;

    const irr::core::dimension2du mainSize = driver->getScreenSize();
    driver->OnResize(size);
    driver->beginScene(irr::video::ECBF_COLOR | irr::video::ECBF_DEPTH, BG, 1.0f, 0, window->videoData());
    driver->setViewPort(irr::core::rect<irr::s32>(0, 0, (irr::s32)size.Width, (irr::s32)size.Height));
    draw(size.Width, size.Height);
    driver->endScene();
    driver->OnResize(mainSize);
}

void SonarWindow::layout(irr::u32 w, irr::u32 h)
{
    k = irr::core::clamp(h / 1000.0f, 0.6f, 2.2f);
    if (std::fabs(k - fontScale) > 0.04f || !fontText) { loadFonts(k); }

    const irr::f32 W = (irr::f32)w, H = (irr::f32)h;
    header = irr::core::rect<irr::f32>(0, 0, W, 58 * k);
    const irr::f32 panelW = irr::core::clamp(W * 0.24f, 230 * k, 320 * k);
    panel = irr::core::rect<irr::f32>(W - panelW, header.LowerRightCorner.Y, W, H);

    const irr::f32 m = 14 * k;
    const irr::core::rect<irr::f32> display(m, header.LowerRightCorner.Y + m, panel.UpperLeftCorner.X - m, H - m);
    const irr::f32 dw = display.getWidth(), dh = display.getHeight();
    area360 = irr::core::rect<irr::f32>(0, 0, 0, 0);
    areaEcho = irr::core::rect<irr::f32>(0, 0, 0, 0);
    if (mode == MODE_360) {
        const irr::f32 s = irr::core::min_(dw, dh);
        area360 = irr::core::rect<irr::f32>(display.UpperLeftCorner.X + (dw - s) / 2, display.UpperLeftCorner.Y + (dh - s) / 2,
            display.UpperLeftCorner.X + (dw + s) / 2, display.UpperLeftCorner.Y + (dh + s) / 2);
    }
    else if (mode == MODE_SOUNDER) {
        areaEcho = display;
    }
    else {
        const irr::f32 s = irr::core::min_(dh, dw * 0.52f);
        area360 = irr::core::rect<irr::f32>(display.UpperLeftCorner.X, display.UpperLeftCorner.Y + (dh - s) / 2,
            display.UpperLeftCorner.X + s, display.UpperLeftCorner.Y + (dh + s) / 2);
        areaEcho = irr::core::rect<irr::f32>(area360.LowerRightCorner.X + m, display.UpperLeftCorner.Y, display.LowerRightCorner.X, display.LowerRightCorner.Y);
    }
    if (area360.getWidth() > 0) {
        centre360 = area360.getCenter();
        radius360 = area360.getWidth() * 0.5f - 30 * k;
    }
    if (areaEcho.getWidth() > 0) {
        const irr::f32 top = areaEcho.UpperLeftCorner.Y + 34 * k;
        const irr::f32 scaleW = 46 * k, ascopeW = 40 * k;
        scaleEcho = irr::core::rect<irr::f32>(areaEcho.LowerRightCorner.X - scaleW, top, areaEcho.LowerRightCorner.X, areaEcho.LowerRightCorner.Y);
        ascopeEcho = irr::core::rect<irr::f32>(scaleEcho.UpperLeftCorner.X - ascopeW - 4 * k, top, scaleEcho.UpperLeftCorner.X - 4 * k, areaEcho.LowerRightCorner.Y);
        picEcho = irr::core::rect<irr::f32>(areaEcho.UpperLeftCorner.X, top, ascopeEcho.UpperLeftCorner.X - 6 * k, areaEcho.LowerRightCorner.Y);
    }

}

//Header keys: the modes, then full screen and close on the right
void SonarWindow::addHeaderButtons()
{
    const irr::f32 W = header.LowerRightCorner.X;
    const irr::f32 kh = 36 * k, ky = (header.getHeight() - kh) / 2;
    irr::f32 x = 150 * k;
    const wchar_t* modeNames[3] = { L"SONAR 360\u00B0", L"SONDEUR", L"LES DEUX" };
    const Action modeActions[3] = { A_MODE_360, A_MODE_SOUNDER, A_MODE_BOTH };
    for (int i = 0; i < 3; i++) {
        const irr::f32 bw = fontText->width(modeNames[i]) + 34 * k;
        addButton(irr::core::rect<irr::f32>(x, ky, x + bw, ky + kh), modeActions[i], modeNames[i], (int)mode == i);
        x += bw + 6 * k;
    }
    irr::f32 xr = W - 12 * k;
    {
        const std::wstring t = L"FERMER";
        const irr::f32 bw = fontText->width(t) + 34 * k;
        addButton(irr::core::rect<irr::f32>(xr - bw, ky, xr, ky + kh), A_CLOSE, t);
        xr -= bw + 6 * k;
    }
    if (!onScreen) {
        const std::wstring t = window && window->isFullScreen() ? L"FEN\u00CATRE" : L"PLEIN \u00C9CRAN";
        const irr::f32 bw = fontText->width(L"PLEIN \u00C9CRAN") + 34 * k;
        addButton(irr::core::rect<irr::f32>(xr - bw, ky, xr, ky + kh), A_FULLSCREEN, t);
    }
}

//-------------------------------------------------------------------------------------------------
//Input
//-------------------------------------------------------------------------------------------------
void SonarWindow::handleInput()
{
    std::vector<irr::SEvent> events;
    bool closeRequested = false;
    window->poll(events, closeRequested);
    if (closeRequested) {
        close();
        return;
    }
    for (size_t i = 0; i < events.size(); i++) {
        const irr::SEvent& e = events[i];
        if (e.EventType != irr::EET_MOUSE_INPUT_EVENT) { continue; }
        mouse = irr::core::position2di(e.MouseInput.X, e.MouseInput.Y);
        mouseInside = true;
        const irr::core::vector2df p((irr::f32)mouse.X, (irr::f32)mouse.Y);
        const bool on360 = area360.getWidth() > 0 && (p - centre360).getLength() <= radius360;
        const bool onEcho = areaEcho.getWidth() > 0 && picEcho.isPointInside(p);
        switch (e.MouseInput.Event) {
        case irr::EMIE_LMOUSE_PRESSED_DOWN: {
            bool done = false;
            for (size_t b = 0; b < buttons.size() && !done; b++) {
                if (buttons[b].r.isPointInside(p)) { act(buttons[b].action); done = true; }
            }
            if (!done && on360) { setMarkerAt(mouse); }
            if (!isOpen()) { return; }
            break;
        }
        case irr::EMIE_RMOUSE_PRESSED_DOWN:
            if (on360) { markerOn = false; }
            break;
        case irr::EMIE_MOUSE_WHEEL:
            if (on360) { act(e.MouseInput.Wheel > 0 ? A_RANGE_DOWN : A_RANGE_UP); }
            else if (onEcho) { act(e.MouseInput.Wheel > 0 ? A_SRANGE_DOWN : A_SRANGE_UP); }
            break;
        default:
            break;
        }
    }
}

void SonarWindow::act(Action a)
{
    switch (a) {
    case A_MODE_360: mode = MODE_360; break;
    case A_MODE_SOUNDER: mode = MODE_SOUNDER; break;
    case A_MODE_BOTH: mode = MODE_BOTH; break;
    case A_FULLSCREEN: fullScreenRequest = true; break;
    case A_CLOSE: close(); return;
    case A_RANGE_DOWN: sonar.setRangeIndex360(sonar.rangeIndex360() - 1); break;
    case A_RANGE_UP: sonar.setRangeIndex360(sonar.rangeIndex360() + 1); break;
    case A_TILT_DOWN: sonar.setTilt(sonar.tilt() - 1.0f); break;
    case A_TILT_UP: sonar.setTilt(sonar.tilt() + 1.0f); break;
    case A_GAIN360_DOWN: sonar.setGain360(sonar.gain360() - 1); break;
    case A_GAIN360_UP: sonar.setGain360(sonar.gain360() + 1); break;
    case A_NORTH_UP: headUp = false; break;
    case A_HEAD_UP: headUp = true; break;
    case A_CLEAR_MARKER: markerOn = false; break;
    case A_SRANGE_DOWN: sonar.setSounderAuto(false); sonar.setSounderRangeIndex(sonar.sounderRangeIndex() - 1); break;
    case A_SRANGE_UP: sonar.setSounderAuto(false); sonar.setSounderRangeIndex(sonar.sounderRangeIndex() + 1); break;
    case A_SAUTO: sonar.setSounderAuto(!sonar.sounderAuto()); break;
    case A_SGAIN_DOWN: sonar.setSounderGain(sonar.sounderGain() - 1); break;
    case A_SGAIN_UP: sonar.setSounderGain(sonar.sounderGain() + 1); break;
    case A_FREQ50: sonar.setLowFrequency(true); break;
    case A_FREQ200: sonar.setLowFrequency(false); break;
    case A_SCROLL_SLOWER: sonar.setScrollSpeed(sonar.scrollSpeed() * 2); break;
    case A_SCROLL_FASTER: sonar.setScrollSpeed(sonar.scrollSpeed() / 2); break;
    case A_CLUTTER_DOWN: sonar.setClutter(sonar.clutter() - 1); break;
    case A_CLUTTER_UP: sonar.setClutter(sonar.clutter() + 1); break;
    default: break;
    }
}

void SonarWindow::setMarkerAt(irr::core::position2di p)
{
    const irr::f32 dx = p.X - centre360.X, dy = p.Y - centre360.Y;
    const irr::f32 r = std::sqrt(dx * dx + dy * dy);
    if (r > radius360 || radius360 <= 0) { return; }
    irr::f32 a = std::atan2(dx, -dy) * irr::core::RADTODEG;
    if (headUp) { a += model->getHeading(); }
    markerBearing = std::fmod(a + 720.0f, 360.0f);
    markerRange = r / radius360 * sonar.range360m();
    markerOn = true;
}

//-------------------------------------------------------------------------------------------------
//Pictures
//-------------------------------------------------------------------------------------------------
void SonarWindow::updateTexture360()
{
    const irr::f32 heading = model->getHeading();
    const irr::u32 now = device->getTimer()->getRealTime();
    if (sonar.picture360Serial() == serial360Drawn && (!headUp || std::fabs(heading - headingDrawn) < 0.2f)) { return; }
    if (tex360 && now - lastTex360Ms < 60) { return; }   //(at most about 15 a second)
    if (!tex360) { tex360 = makeTexture("SonarPicture360"); if (!tex360) { return; } }
    irr::u8* out = (irr::u8*)tex360->lock(irr::video::ETLM_WRITE_ONLY);
    if (!out) { return; }
    lastTex360Ms = now;
    serial360Drawn = sonar.picture360Serial();
    headingDrawn = heading;
    const irr::u32 pitch = tex360->getPitch();
    const std::vector<irr::u8>& pic = sonar.picture360();
    const int rot = headUp ? ((int)(heading * 10.0f) % 3600 + 3600) % 3600 : 0;
    const irr::u32 outside = 0xFF000000u | ((irr::u32)CARD.getRed() << 16) | ((irr::u32)CARD.getGreen() << 8) | CARD.getBlue();
    const int perBeam = 3600 / Sonar::BEAMS;
    for (int y = 0; y < TEX; y++) {
        irr::u32* row = (irr::u32*)(out + (size_t)y * pitch);
        const size_t base = (size_t)y * TEX;
        for (int x = 0; x < TEX; x++) {
            const irr::u16 a = lutAngle[base + x];
            if (a == 0xFFFF) { row[x] = outside; continue; }
            const int beam = ((a + rot) % 3600) / perBeam;
            row[x] = palette[pic[(size_t)beam * Sonar::BINS + lutBin[base + x]]];
        }
    }
    tex360->unlock();
}

void SonarWindow::updateTextureEcho()
{
    if (texEcho && sonar.echogramSerial() == serialEchoDrawn) { return; }
    if (!texEcho) { texEcho = makeTexture("SonarEchogram"); if (!texEcho) { return; } }
    irr::u8* out = (irr::u8*)texEcho->lock(irr::video::ETLM_WRITE_ONLY);
    if (!out) { return; }
    serialEchoDrawn = sonar.echogramSerial();
    const irr::u32 pitch = texEcho->getPitch();
    const int n = sonar.columnCount();
    const int firstX = TEX - n;   //newest on the right
    const irr::u32 empty = palette[0];
    for (int y = 0; y < TEX; y++) {
        irr::u32* row = (irr::u32*)(out + (size_t)y * pitch);
        for (int x = 0; x < firstX; x++) { row[x] = empty; }
    }
    for (int i = 0; i < n; i++) {
        const Sonar::Column& c = sonar.column(i);
        const int x = firstX + i;
        for (int y = 0; y < TEX; y++) {
            irr::u32* row = (irr::u32*)(out + (size_t)y * pitch);
            //A change of range: a dotted line down the column
            row[x] = (c.rangeChanged && (y % 6) < 3) ? 0xFFB0B8C8u : palette[c.echo[y]];
        }
    }
    texEcho->unlock();
}

//-------------------------------------------------------------------------------------------------
//Drawing
//-------------------------------------------------------------------------------------------------
void SonarWindow::line(irr::f32 x0, irr::f32 y0, irr::f32 x1, irr::f32 y1, irr::video::SColor c)
{
    driver->draw2DLine(irr::core::position2di((irr::s32)x0, (irr::s32)y0), irr::core::position2di((irr::s32)x1, (irr::s32)y1), c);
}

void SonarWindow::circle(irr::f32 cx, irr::f32 cy, irr::f32 r, irr::video::SColor c, int segments)
{
    for (int i = 0; i < segments; i++) {
        const irr::f32 a0 = i * 2.0f * irr::core::PI / segments, a1 = (i + 1) * 2.0f * irr::core::PI / segments;
        line(cx + r * std::sin(a0), cy - r * std::cos(a0), cx + r * std::sin(a1), cy - r * std::cos(a1), c);
    }
}

void SonarWindow::addButton(const irr::core::rect<irr::f32>& r, Action a, const std::wstring& label, bool selected)
{
    Button b;
    b.r = r;
    b.action = a;
    b.label = label;
    b.selected = selected;
    buttons.push_back(b);
}

void SonarWindow::drawButtons()
{
    for (size_t i = 0; i < buttons.size(); i++) {
        const Button& b = buttons[i];
        const bool hover = mouseInside && b.r.isPointInside(irr::core::vector2df((irr::f32)mouse.X, (irr::f32)mouse.Y));
        driver->draw2DRectangle(b.selected ? KEY_ON : (hover ? KEY_HOVER : KEY), toInt(b.r));
        driver->draw2DRectangleOutline(toInt(b.r), b.selected ? ACCENT : CARD_LINE);
        fontText->drawIn(b.label, b.r, b.selected ? irr::video::SColor(255, 255, 255, 255) : TEXT, HudFont::Centre);
    }
}

irr::f32 SonarWindow::panelRow(irr::f32 y, const std::wstring& label, const std::wstring& value, Action down, Action up)
{
    const irr::f32 x0 = panel.UpperLeftCorner.X + 16 * k, x1 = panel.LowerRightCorner.X - 16 * k;
    const irr::f32 h = 34 * k;
    const irr::f32 keyW = 34 * k;
    const irr::f32 labelW = (x1 - x0) * 0.42f;
    fontText->drawIn(label, irr::core::rect<irr::f32>(x0, y, x0 + labelW, y + h), DIM, HudFont::Left);
    const irr::f32 vx0 = x0 + labelW;
    addButton(irr::core::rect<irr::f32>(vx0, y, vx0 + keyW, y + h), down, L"-");
    addButton(irr::core::rect<irr::f32>(x1 - keyW, y, x1, y + h), up, L"+");
    driver->draw2DRectangle(BG, toInt(irr::core::rect<irr::f32>(vx0 + keyW + 3 * k, y, x1 - keyW - 3 * k, y + h)));
    fontText->drawIn(value, irr::core::rect<irr::f32>(vx0 + keyW, y, x1 - keyW, y + h), TEXT, HudFont::Centre);
    return y + h + 8 * k;
}

void SonarWindow::draw(irr::u32 w, irr::u32 h)
{
    driver->draw2DRectangle(BG, irr::core::rect<irr::s32>(0, 0, (irr::s32)w, (irr::s32)h));
    //The keys are made as they are drawn; the mouse finds them until the next frame
    buttons.clear();
    addHeaderButtons();
    if (area360.getWidth() > 0) { updateTexture360(); draw360(); }
    if (areaEcho.getWidth() > 0) { updateTextureEcho(); drawSounder(); }
    drawPanel();
    drawHeader();
    drawButtons();
}

void SonarWindow::drawHeader()
{
    driver->draw2DRectangle(HEADER, toInt(header));
    line(header.UpperLeftCorner.X, header.LowerRightCorner.Y - 1, header.LowerRightCorner.X, header.LowerRightCorner.Y - 1, CARD_LINE);
    //Sound waves for an icon, then the name
    const irr::f32 cy = header.getCenter().Y;
    for (int i = 1; i <= 3; i++) {
        const irr::f32 r = 5.0f * k * i;
        for (int s = -3; s < 3; s++) {
            const irr::f32 a0 = (90.0f + s * 12.0f) * irr::core::DEGTORAD, a1 = (90.0f + (s + 1) * 12.0f) * irr::core::DEGTORAD;
            line(22 * k + r * std::cos(a0), cy - 6 * k + r * std::sin(a0), 22 * k + r * std::cos(a1), cy - 6 * k + r * std::sin(a1), ACCENT);
        }
    }
    fontTitle->drawIn(L"SONAR", irr::core::rect<irr::f32>(44 * k, 0, 150 * k, header.getHeight()), TEXT, HudFont::Left, 1.5f * k);
}

void SonarWindow::drawPanel()
{
    driver->draw2DRectangle(CARD, toInt(panel));
    line(panel.UpperLeftCorner.X, panel.UpperLeftCorner.Y, panel.UpperLeftCorner.X, panel.LowerRightCorner.Y, CARD_LINE);
    const irr::f32 x0 = panel.UpperLeftCorner.X + 16 * k, x1 = panel.LowerRightCorner.X - 16 * k;
    irr::f32 y = panel.UpperLeftCorner.Y + 16 * k;
    auto section = [&](const std::wstring& title) {
        fontSmall->drawIn(title, irr::core::rect<irr::f32>(x0, y, x1, y + 20 * k), ACCENT, HudFont::Left, 1.2f * k);
        y += 22 * k;
        line(x0, y, x1, y, CARD_LINE);
        y += 10 * k;
    };
    auto twoKeys = [&](const std::wstring& label, const std::wstring& a, Action aa, bool aOn, const std::wstring& b, Action ba, bool bOn) {
        const irr::f32 h = 34 * k;
        const irr::f32 labelW = (x1 - x0) * 0.42f;
        fontText->drawIn(label, irr::core::rect<irr::f32>(x0, y, x0 + labelW, y + h), DIM, HudFont::Left);
        const irr::f32 kx = x0 + labelW, kw = (x1 - kx - 4 * k) / 2;
        addButton(irr::core::rect<irr::f32>(kx, y, kx + kw, y + h), aa, a, aOn);
        addButton(irr::core::rect<irr::f32>(kx + kw + 4 * k, y, x1, y + h), ba, b, bOn);
        y += h + 8 * k;
    };

    if (mode != MODE_SOUNDER) {
        section(L"SONAR 360\u00B0");
        y = panelRow(y, L"Port\u00E9e", metres(sonar.range360m()), A_RANGE_DOWN, A_RANGE_UP);
        y = panelRow(y, L"Inclinaison", fmt(L"%.0f\u00B0", sonar.tilt()), A_TILT_DOWN, A_TILT_UP);
        y = panelRow(y, L"Gain", fmt(L"%.0f", sonar.gain360()), A_GAIN360_DOWN, A_GAIN360_UP);
        twoKeys(L"Orientation", L"NORD", A_NORTH_UP, !headUp, L"CAP", A_HEAD_UP, headUp);
        if (markerOn) {
            const irr::f32 h = 34 * k;
            addButton(irr::core::rect<irr::f32>(x0, y, x1, y + h), A_CLEAR_MARKER, L"EFFACER LE MARQUEUR");
            y += h + 8 * k;
        }
        y += 8 * k;
    }
    if (mode != MODE_360) {
        section(L"SONDEUR");
        const std::wstring scale = (sonar.sounderAuto() ? L"AUTO " : L"") + metres(sonar.sounderRange());
        y = panelRow(y, L"\u00C9chelle", scale, A_SRANGE_DOWN, A_SRANGE_UP);
        {
            const irr::f32 h = 34 * k;
            const irr::f32 labelW = (x1 - x0) * 0.42f;
            addButton(irr::core::rect<irr::f32>(x0 + labelW, y, x1, y + h), A_SAUTO, L"\u00C9CHELLE AUTOMATIQUE", sonar.sounderAuto());
            y += h + 8 * k;
        }
        y = panelRow(y, L"Gain", fmt(L"%.0f", sonar.sounderGain()), A_SGAIN_DOWN, A_SGAIN_UP);
        twoKeys(L"Fr\u00E9quence", L"50 kHz", A_FREQ50, sonar.lowFrequency(), L"200 kHz", A_FREQ200, !sonar.lowFrequency());
        const int sc = sonar.scrollSpeed();
        y = panelRow(y, L"D\u00E9filement", (sc == 1) ? L"rapide" : ((sc == 2) ? L"moyen" : L"lent"), A_SCROLL_SLOWER, A_SCROLL_FASTER);
        y = panelRow(y, L"Anti-parasites", fmt(L"%.0f", sonar.clutter()), A_CLUTTER_DOWN, A_CLUTTER_UP);
        //Colours, weak to strong
        const irr::f32 bh = 10 * k;
        fontSmall->drawIn(L"faible", irr::core::rect<irr::f32>(x0, y, x0 + 60 * k, y + bh * 2), DIM, HudFont::Left);
        fontSmall->drawIn(L"fort", irr::core::rect<irr::f32>(x1 - 60 * k, y, x1, y + bh * 2), DIM, HudFont::Right);
        y += bh * 2 + 2 * k;
        const irr::f32 lw = x1 - x0;
        for (int i = 0; i < 64; i++) {
            const irr::u32 c = palette[i * 4 + 2];
            driver->draw2DRectangle(irr::video::SColor(c), toInt(irr::core::rect<irr::f32>(x0 + lw * i / 64.0f, y, x0 + lw * (i + 1) / 64.0f + 1, y + bh)));
        }
        y += bh + 14 * k;
    }

    //The ship, at the bottom
    const irr::f32 boxH = 128 * k;
    const irr::f32 by = irr::core::max_(y, panel.LowerRightCorner.Y - boxH - 16 * k);
    const irr::core::rect<irr::f32> box(x0, by, x1, by + boxH);
    driver->draw2DRectangle(BG, toInt(box));
    driver->draw2DRectangleOutline(toInt(box), CARD_LINE);
    const irr::f32 half = (x1 - x0) / 2;
    fontSmall->drawIn(L"CAP", irr::core::rect<irr::f32>(x0 + 10 * k, by + 8 * k, x0 + half, by + 26 * k), DIM, HudFont::Left);
    fontSmall->drawIn(L"VITESSE", irr::core::rect<irr::f32>(x0 + half, by + 8 * k, x1 - 10 * k, by + 26 * k), DIM, HudFont::Left);
    fontText->drawIn(fmt(L"%03.0f\u00B0", std::fmod(model->getHeading() + 360.0f, 360.0f)), irr::core::rect<irr::f32>(x0 + 10 * k, by + 26 * k, x0 + half, by + 52 * k), TEXT, HudFont::Left);
    fontText->drawIn(fmt(L"%.1f nd", model->getSOG() * 1.943844f), irr::core::rect<irr::f32>(x0 + half, by + 26 * k, x1 - 10 * k, by + 52 * k), TEXT, HudFont::Left);
    fontSmall->drawIn(L"PROFONDEUR", irr::core::rect<irr::f32>(x0 + 10 * k, by + 60 * k, x1, by + 78 * k), DIM, HudFont::Left);
    const irr::f32 d = sonar.lastDepth();
    fontBig->drawIn(d > 0 ? fmt(L"%.1f m", d) : std::wstring(L"---"), irr::core::rect<irr::f32>(x0 + 10 * k, by + 80 * k, x1 - 10 * k, by + boxH - 8 * k), irr::video::SColor(255, 255, 214, 90), HudFont::Left);
}

void SonarWindow::draw360()
{
    const irr::f32 cx = centre360.X, cy = centre360.Y, R = radius360;
    const irr::f32 heading = model->getHeading();
    const irr::f32 rot = headUp ? heading : 0.0f;   //screen angle of a true bearing: bearing - rot
    driver->draw2DRectangle(CARD, toInt(area360));

    //The picture, smoothed when scaled
    if (tex360) {
        driver->getMaterial2D().TextureLayer[0].BilinearFilter = true;
        driver->enableMaterial2D(true);
        driver->draw2DImage(tex360, toInt(irr::core::rect<irr::f32>(cx - R, cy - R, cx + R, cy + R)),
            irr::core::rect<irr::s32>(0, 0, TEX, TEX));
        driver->enableMaterial2D(false);
    }

    //Range rings and their distances
    const irr::f32 range = sonar.range360m();
    for (int i = 1; i <= 4; i++) {
        const irr::f32 r = R * i / 4.0f;
        circle(cx, cy, r, (i == 4) ? irr::video::SColor(220, 170, 200, 230) : RING, 144);
        const irr::f32 a = 135.0f * irr::core::DEGTORAD;
        fontSmall->draw(metres(range * i / 4.0f), cx + r * std::sin(a) + 4 * k, cy - r * std::cos(a) + 2 * k, irr::video::SColor(200, 190, 210, 230));
    }
    //Bearing scale (true bearings)
    for (int b = 0; b < 360; b += 5) {
        const irr::f32 a = (b - rot) * irr::core::DEGTORAD;
        const irr::f32 l = (b % 30 == 0) ? 12 * k : ((b % 10 == 0) ? 8 * k : 4 * k);
        line(cx + R * std::sin(a), cy - R * std::cos(a), cx + (R + l) * std::sin(a), cy - (R + l) * std::cos(a), irr::video::SColor(220, 170, 200, 230));
        if (b % 30 == 0) {
            const irr::f32 lr = R + 22 * k;
            const irr::core::rect<irr::f32> box(cx + lr * std::sin(a) - 20 * k, cy - lr * std::cos(a) - 9 * k, cx + lr * std::sin(a) + 20 * k, cy - lr * std::cos(a) + 9 * k);
            fontSmall->drawIn(fmt(L"%03.0f", b), box, (b == 0) ? ACCENT : irr::video::SColor(220, 190, 210, 230), HudFont::Centre);
        }
    }
    //The sweep, the ship's head (dashed), the ship
    {
        const irr::f32 a = (sonar.sweepBearing() - rot) * irr::core::DEGTORAD;
        line(cx, cy, cx + R * std::sin(a), cy - R * std::cos(a), SWEEP);
        const irr::f32 h = (heading - rot) * irr::core::DEGTORAD;
        for (int i = 0; i < 20; i += 2) {
            const irr::f32 r0 = R * i / 20.0f, r1 = R * (i + 1) / 20.0f;
            line(cx + r0 * std::sin(h), cy - r0 * std::cos(h), cx + r1 * std::sin(h), cy - r1 * std::cos(h), irr::video::SColor(230, 255, 255, 255));
        }
        const irr::f32 s = 9 * k;
        const irr::core::vector2df tip(cx + s * std::sin(h), cy - s * std::cos(h));
        const irr::core::vector2df l(cx + s * 0.7f * std::sin(h + 2.5f), cy - s * 0.7f * std::cos(h + 2.5f));
        const irr::core::vector2df r(cx + s * 0.7f * std::sin(h - 2.5f), cy - s * 0.7f * std::cos(h - 2.5f));
        line(tip.X, tip.Y, l.X, l.Y, irr::video::SColor(255, 255, 255, 255));
        line(l.X, l.Y, r.X, r.Y, irr::video::SColor(255, 255, 255, 255));
        line(r.X, r.Y, tip.X, tip.Y, irr::video::SColor(255, 255, 255, 255));
    }
    //The marker: bearing, slant range, horizontal distance and estimated depth
    if (markerOn) {
        const irr::f32 a = (markerBearing - rot) * irr::core::DEGTORAD;
        const irr::f32 r = R * irr::core::min_(markerRange / range, 1.0f);
        const irr::f32 mx = cx + r * std::sin(a), my = cy - r * std::cos(a);
        line(mx - 10 * k, my, mx - 3 * k, my, MARK); line(mx + 3 * k, my, mx + 10 * k, my, MARK);
        line(mx, my - 10 * k, mx, my - 3 * k, MARK); line(mx, my + 3 * k, mx, my + 10 * k, MARK);
        circle(mx, my, 6 * k, MARK, 24);
        const irr::f32 t = sonar.tilt() * irr::core::DEGTORAD;
        const irr::f32 bw = 150 * k, bh = 92 * k;
        const irr::core::rect<irr::f32> box(area360.LowerRightCorner.X - bw - 6 * k, area360.UpperLeftCorner.Y + 6 * k, area360.LowerRightCorner.X - 6 * k, area360.UpperLeftCorner.Y + 6 * k + bh);
        driver->draw2DRectangle(irr::video::SColor(220, 8, 14, 26), toInt(box));
        driver->draw2DRectangleOutline(toInt(box), MARK);
        irr::f32 ty = box.UpperLeftCorner.Y + 6 * k;
        const irr::f32 tx = box.UpperLeftCorner.X + 8 * k;
        fontSmall->draw(L"MARQUEUR", tx, ty, MARK); ty += 20 * k;
        fontSmall->draw(fmt(L"Rel\u00E8vement  %03.0f\u00B0", markerBearing), tx, ty, TEXT); ty += 17 * k;
        fontSmall->draw(L"Distance  " + metres(markerRange * std::cos(t)), tx, ty, TEXT); ty += 17 * k;
        fontSmall->draw(fmt(L"Profondeur  ~ %.0f m", markerRange * std::sin(t) + sonar.transducerDepth()), tx, ty, TEXT);
    }
    //Top left: the settings; bottom left: the tilt of the beam seen from the side
    {
        irr::f32 ty = area360.UpperLeftCorner.Y + 6 * k;
        const irr::f32 tx = area360.UpperLeftCorner.X + 8 * k;
        fontSmall->draw(L"PORT\u00C9E  " + metres(range), tx, ty, TEXT); ty += 17 * k;
        fontSmall->draw(fmt(L"GAIN  %.0f", sonar.gain360()), tx, ty, TEXT); ty += 17 * k;
        fontSmall->draw(headUp ? L"CAP EN HAUT" : L"NORD EN HAUT", tx, ty, ACCENT);

        const irr::f32 bw = 120 * k, bh = 70 * k;
        const irr::core::rect<irr::f32> box(area360.UpperLeftCorner.X + 6 * k, area360.LowerRightCorner.Y - bh - 6 * k, area360.UpperLeftCorner.X + 6 * k + bw, area360.LowerRightCorner.Y - 6 * k);
        driver->draw2DRectangle(irr::video::SColor(220, 8, 14, 26), toInt(box));
        driver->draw2DRectangleOutline(toInt(box), CARD_LINE);
        const irr::f32 sx = box.UpperLeftCorner.X + 12 * k, sy = box.UpperLeftCorner.Y + 26 * k;
        line(box.UpperLeftCorner.X + 4 * k, sy - 4 * k, box.LowerRightCorner.X - 4 * k, sy - 4 * k, irr::video::SColor(255, 90, 150, 210)); //surface
        const irr::f32 len = bw - 24 * k;
        const irr::f32 ta = sonar.tilt() * irr::core::DEGTORAD, hv = 4.0f * irr::core::DEGTORAD;
        line(sx, sy, sx + len * std::cos(ta - hv), sy + len * std::sin(ta - hv), irr::video::SColor(160, 120, 255, 170));
        line(sx, sy, sx + len * std::cos(ta + hv), sy + len * std::sin(ta + hv), irr::video::SColor(160, 120, 255, 170));
        line(sx, sy, sx + len * std::cos(ta), sy + len * std::sin(ta), SWEEP);
        fontSmall->draw(fmt(L"INCLINAISON %.0f\u00B0", sonar.tilt()), box.UpperLeftCorner.X + 8 * k, box.UpperLeftCorner.Y + 4 * k, TEXT);
    }
}

void SonarWindow::drawSounder()
{
    driver->draw2DRectangle(CARD, toInt(areaEcho));
    const irr::f32 R = sonar.sounderRange();

    //Top line: depth and settings
    {
        const irr::f32 ty = areaEcho.UpperLeftCorner.Y + 6 * k;
        std::wstring info = sonar.lowFrequency() ? L"50 kHz" : L"200 kHz";
        info += fmt(L"    GAIN %.0f", sonar.sounderGain());
        info += L"    \u00C9CHELLE " + std::wstring(sonar.sounderAuto() ? L"AUTO " : L"") + metres(R);
        fontSmall->draw(info, areaEcho.UpperLeftCorner.X + 8 * k, ty + 4 * k, TEXT);
    }

    //The echogram, newest on the right
    if (texEcho) {
        driver->getMaterial2D().TextureLayer[0].BilinearFilter = true;
        driver->enableMaterial2D(true);
        driver->draw2DImage(texEcho, toInt(picEcho), irr::core::rect<irr::s32>(0, 0, TEX, TEX));
        driver->enableMaterial2D(false);
    }
    driver->draw2DRectangleOutline(toInt(picEcho), CARD_LINE);

    //Depth scale
    const irr::f32 top = picEcho.UpperLeftCorner.Y, hgt = picEcho.getHeight();
    const irr::f32 step = niceStep(R, 6);
    for (irr::f32 d = 0; d <= R + 0.01f; d += step) {
        const irr::f32 y = top + hgt * d / R;
        line(picEcho.LowerRightCorner.X - 8 * k, y, picEcho.LowerRightCorner.X, y, RING);
        line(scaleEcho.UpperLeftCorner.X, y, scaleEcho.UpperLeftCorner.X + 8 * k, y, RING);
        const irr::core::rect<irr::f32> box(scaleEcho.UpperLeftCorner.X + 10 * k, y - 9 * k, scaleEcho.LowerRightCorner.X, y + 9 * k);
        fontSmall->drawIn(fmt(step < 1.0f ? L"%.1f" : L"%.0f", d), box, TEXT, HudFont::Left);
    }

    //A-scope: the last column as a trace
    driver->draw2DRectangle(PICTURE_BG, toInt(ascopeEcho));
    if (sonar.columnCount() > 0) {
        const Sonar::Column& c = sonar.column(sonar.columnCount() - 1);
        const irr::f32 aw = ascopeEcho.getWidth();
        for (int r = 0; r < Sonar::ROWS; r += 2) {
            const irr::u8 v = irr::core::max_(c.echo[r], c.echo[r + 1]);
            if (v < 8) { continue; }
            const irr::f32 y0 = top + hgt * r / Sonar::ROWS, y1 = top + hgt * (r + 2) / Sonar::ROWS;
            const irr::f32 len = aw * v / 255.0f;
            driver->draw2DRectangle(irr::video::SColor(palette[v]), toInt(irr::core::rect<irr::f32>(ascopeEcho.UpperLeftCorner.X, y0, ascopeEcho.UpperLeftCorner.X + len, irr::core::max_(y1, y0 + 1))));
        }
    }
    driver->draw2DRectangleOutline(toInt(ascopeEcho), CARD_LINE);

    //Depth now, big, over the picture
    {
        const irr::f32 d = sonar.lastDepth();
        const irr::core::rect<irr::f32> box(picEcho.UpperLeftCorner.X + 8 * k, top + 8 * k, picEcho.UpperLeftCorner.X + 170 * k, top + 66 * k);
        driver->draw2DRectangle(irr::video::SColor(170, 4, 10, 30), toInt(box));
        fontBig->drawIn(d > 0 ? fmt(L"%.1f m", d) : std::wstring(L"---"), box, irr::video::SColor(255, 255, 255, 255), HudFont::Centre);
    }

    //Under the mouse: a depth line
    const irr::core::vector2df p((irr::f32)mouse.X, (irr::f32)mouse.Y);
    if (mouseInside && picEcho.isPointInside(p)) {
        const irr::f32 d = (p.Y - top) / hgt * R;
        line(picEcho.UpperLeftCorner.X, p.Y, picEcho.LowerRightCorner.X, p.Y, MARK);
        const std::wstring t = fmt(L"%.1f m", d);
        const irr::f32 tw = fontSmall->width(t) + 12 * k;
        const irr::core::rect<irr::f32> box(picEcho.LowerRightCorner.X - tw - 10 * k, p.Y - 20 * k, picEcho.LowerRightCorner.X - 10 * k, p.Y - 2 * k);
        driver->draw2DRectangle(irr::video::SColor(200, 8, 14, 26), toInt(box));
        fontSmall->drawIn(t, box, MARK, HudFont::Centre);
    }
}
