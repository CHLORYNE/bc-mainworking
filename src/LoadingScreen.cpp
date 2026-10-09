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

#include "LoadingScreen.hpp"
#include "BridgeSkin.hpp"
#include "GUIPanelDraw.hpp"
#include "launcher/HudText.hpp"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
    using irr::video::SColor;
    using irr::core::vector2df;
    typedef irr::core::rect<irr::f32> RectF;

    //Night-bridge palette: deep navy glass, cyan instruments
    const SColor COL_ACCENT(255, 0, 176, 222);
    const SColor COL_ACCENT_HI(255, 120, 226, 250);
    const SColor COL_TEXT(255, 240, 245, 250);
    const SColor COL_SOFT(255, 196, 212, 228);
    const SColor COL_MUTED(255, 128, 154, 178);
    const SColor COL_DIM(255, 84, 104, 124);

    SColor alpha(SColor c, irr::u32 a) { c.setAlpha(a); return c; }

    irr::core::rect<irr::s32> toI(const RectF& r)
    {
        return irr::core::rect<irr::s32>((irr::s32)floorf(r.UpperLeftCorner.X + 0.5f), (irr::s32)floorf(r.UpperLeftCorner.Y + 0.5f),
            (irr::s32)floorf(r.LowerRightCorner.X + 0.5f), (irr::s32)floorf(r.LowerRightCorner.Y + 0.5f));
    }

    vector2df polar(const vector2df& c, irr::f32 r, irr::f32 deg) //clockwise from north
    {
        const irr::f32 a = deg * irr::core::DEGTORAD;
        return vector2df(c.X + r * sinf(a), c.Y - r * cosf(a));
    }

    irr::video::ITexture* loadOptionalTexture(irr::IrrlichtDevice* device, const char* path)
    {
        //existFile first: getTexture() on a missing file would print an error in the log
        if (device->getFileSystem()->existFile(path)) {
            return device->getVideoDriver()->getTexture(path);
        }
        return 0;
    }

    //A lighthouse with its beams, in a box of side s at (x, y)
    void drawLighthouse(irr::video::IVideoDriver* driver, irr::f32 x, irr::f32 y, irr::f32 s, SColor ink, SColor dark)
    {
        auto P = [&](irr::f32 px, irr::f32 py) { return vector2df(x + px * s, y + py * s); };
        irr::gui::PanelBatch b;
        b.begin(driver);
        //beams, fading out
        b.tri(P(0.5f, 0.30f), alpha(ink, 170), P(0.02f, 0.16f), alpha(ink, 0), P(0.02f, 0.44f), alpha(ink, 0));
        b.tri(P(0.5f, 0.30f), alpha(ink, 170), P(0.98f, 0.44f), alpha(ink, 0), P(0.98f, 0.16f), alpha(ink, 0));
        //roof and lantern
        b.tri(P(0.5f, 0.10f), P(0.66f, 0.22f), P(0.34f, 0.22f), ink);
        b.rect(RectF(P(0.37f, 0.22f), P(0.63f, 0.36f)), ink);
        b.rect(RectF(P(0.41f, 0.25f), P(0.59f, 0.33f)), SColor(255, 255, 236, 170));
        b.rect(RectF(P(0.33f, 0.36f), P(0.67f, 0.39f)), ink);
        //tower, with two dark bands
        b.quad(P(0.40f, 0.39f), P(0.60f, 0.39f), P(0.68f, 0.92f), P(0.32f, 0.92f), ink);
        b.quad(P(0.384f, 0.50f), P(0.616f, 0.50f), P(0.627f, 0.58f), P(0.373f, 0.58f), dark);
        b.quad(P(0.357f, 0.68f), P(0.643f, 0.68f), P(0.654f, 0.76f), P(0.346f, 0.76f), dark);
        //rock
        b.rect(RectF(P(0.20f, 0.92f), P(0.80f, 0.96f)), ink);
        b.flush();
    }

    //A small tick mark (stage done)
    void drawCheck(irr::video::IVideoDriver* driver, irr::f32 x, irr::f32 y, irr::f32 s, SColor col)
    {
        irr::gui::PanelBatch b;
        b.begin(driver);
        b.line(vector2df(x, y + s * 0.55f), vector2df(x + s * 0.38f, y + s * 0.9f), s * 0.16f, col);
        b.line(vector2df(x + s * 0.38f, y + s * 0.9f), vector2df(x + s, y + s * 0.12f), s * 0.16f, col);
        b.flush();
    }
}

//=================================================================================================

LoadingScreen::LoadingScreen(irr::IrrlichtDevice* device, irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* titleFont)
{
    this->device = device;
    driver = device ? device->getVideoDriver() : 0;

    this->textFont = textFont;
    if (!this->textFont && device) {
        this->textFont = device->getGUIEnvironment()->getBuiltInFont();
    }
    this->titleFont = titleFont ? titleFont : this->textFont;

    background = 0;
    if (device) {
        background = loadOptionalTexture(device, "media/loading_bg.jpg");
        if (!background) { background = loadOptionalTexture(device, "media/loading_bg.png"); }
    }

    progress = 0.0f;
    stageStartTime = now();
    loadStartTime = stageStartTime;
    lastDrawTime = 0;

    k = 1.0f;
    if (driver) {
        const irr::f32 H = (irr::f32)driver->getScreenSize().Height;
        k = irr::core::clamp(H / 1080.0f, 0.5f, 2.5f);
    }
    loadFonts(k);

    loadFacts();
    factIndex = facts.empty() ? 0 : (size_t)(now() % facts.size());
    factShownTime = now();
}

LoadingScreen::~LoadingScreen()
{
    freeFonts();
}

void LoadingScreen::loadFonts(irr::f32 s)
{
    freeFonts();
    if (!driver) { return; }
    const std::string folder = "media/fonts/barlow-condensed/";
    fonts.hero = new HudFont(driver, folder + "BarlowCondensed-Bold.ttf", 92 * s, titleFont);
    fonts.title = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 66 * s, titleFont);
    fonts.titleSmall = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 50 * s, titleFont);
    fonts.value = new HudFont(driver, folder + "BarlowCondensed-Medium.ttf", 31 * s, textFont);
    fonts.body = new HudFont(driver, folder + "BarlowCondensed-Regular.ttf", 25 * s, textFont);
    fonts.fact = new HudFont(driver, folder + "BarlowCondensed-Medium.ttf", 40 * s, textFont);
    fonts.factSmall = new HudFont(driver, folder + "BarlowCondensed-Medium.ttf", 32 * s, textFont);
    fonts.stage = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 27 * s, textFont);
    fonts.kicker = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 19 * s, textFont);
    fonts.small = new HudFont(driver, folder + "BarlowCondensed-Medium.ttf", 17 * s, textFont);
}

void LoadingScreen::freeFonts()
{
    HudFont** all[] = { &fonts.hero, &fonts.title, &fonts.titleSmall, &fonts.value, &fonts.body, &fonts.fact,
        &fonts.factSmall, &fonts.stage, &fonts.kicker, &fonts.small };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        delete *all[i];
        *all[i] = 0;
    }
}

void LoadingScreen::loadFacts()
{
    //media/le_saviez_vous.txt: one fact per line, "N. text". Anything else (title, comments) is skipped.
    std::ifstream file("media/le_saviez_vous.txt", std::ios::binary);
    if (!file) { return; }
    std::string line;
    bool first = true;
    while (std::getline(file, line)) {
        if (first && line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
            line.erase(0, 3); //UTF-8 mark left by Notepad
        }
        first = false;
        size_t i = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) { i++; }
        const size_t digits = i;
        while (i < line.size() && line[i] >= '0' && line[i] <= '9') { i++; }
        if (i == digits || i >= line.size() || (line[i] != '.' && line[i] != ')')) { continue; }
        i++;
        std::wstring text = fromUtf8(line.substr(i));
        while (!text.empty() && (text[0] == L' ' || text[0] == L'\t')) { text.erase(0, 1); }
        while (!text.empty() && (text[text.size() - 1] == L' ' || text[text.size() - 1] == L'\t')) { text.erase(text.size() - 1); }
        if (!text.empty()) { facts.push_back(text); }
    }
}

void LoadingScreen::setTitle(const std::string& titleUtf8)
{
    title = fromUtf8(titleUtf8);
}

void LoadingScreen::addInfo(const std::string& labelUtf8, const std::string& valueUtf8)
{
    if (valueUtf8.empty()) { return; }
    infoLabels.push_back(fromUtf8(labelUtf8));
    infoValues.push_back(fromUtf8(valueUtf8));
}

void LoadingScreen::setBriefing(const std::string& textUtf8)
{
    briefing = fromUtf8(textUtf8);
}

void LoadingScreen::setStage(irr::f32 newProgress, const std::string& stageTextUtf8)
{
    logPreviousStage();
    stageLogName = stageTextUtf8;
    stageStartTime = now();
    if (newProgress < 0.0f) { newProgress = 0.0f; }
    if (newProgress > 1.0f) { newProgress = 1.0f; }
    progress = newProgress;
    const std::wstring next = fromUtf8(stageTextUtf8);
    if (!stageText.empty() && stageText != next) { doneStages.push_back(stageText); }
    stageText = next;
    draw();
}

void LoadingScreen::refresh()
{
    if (now() - lastDrawTime >= 100) {
        draw();
    }
}

void LoadingScreen::finish()
{
    logPreviousStage();
    stageLogName.clear();
    progress = 1.0f;
    if (!stageText.empty()) { doneStages.push_back(stageText); }
    stageText = L"Pr\u00EAt \u2014 bon quart !";
    draw();

    std::ostringstream line;
    line << "[CHARGEMENT] TOTAL : " << (now() - loadStartTime) << " ms";
    std::cout << line.str() << std::endl;
    if (device) { device->getLogger()->log(line.str().c_str()); }

    //The photo and the lettering are only used here - give the memory back to the simulation.
    if (background && driver) {
        driver->removeTexture(background);
        background = 0;
    }
    freeFonts();
}

void LoadingScreen::logPreviousStage()
{
    if (stageLogName.empty()) { return; }
    std::ostringstream line;
    line << "[CHARGEMENT] " << stageLogName << " : " << (now() - stageStartTime) << " ms";
    std::cout << line.str() << std::endl;
    if (device) { device->getLogger()->log(line.str().c_str()); }
}

irr::u32 LoadingScreen::now() const
{
    return device ? device->getTimer()->getRealTime() : 0;
}

//-------------------------------------------------------------------------------------------------
//Drawing
//-------------------------------------------------------------------------------------------------

void LoadingScreen::draw()
{
    if (!device || !driver) { return; }

    //Pump the window messages. Without this, Windows marks the window "Ne repond pas" during a long load.
    device->run();
    lastDrawTime = now();
    if (!fonts.title) { return; } //after finish()

    //A new fact every 8 seconds (the page is redrawn at each stage)
    if (facts.size() > 1 && now() - factShownTime >= 8000) {
        factIndex = (factIndex + 1) % facts.size();
        factShownTime = now();
    }

    const irr::core::dimension2d<irr::u32> ss = driver->getScreenSize();
    const irr::s32 W = (irr::s32)ss.Width;
    const irr::s32 H = (irr::s32)ss.Height;
    if (W <= 0 || H <= 0) { return; }

    //Content column: never wider than 16:9 of the height, centred. On a triple-screen (Surround)
    //surface this puts everything on the middle screen; the side screens get the photo's edges.
    irr::s32 contentW = W;
    if (contentW > H * 16 / 9) { contentW = H * 16 / 9; }
    const irr::f32 cx0 = (irr::f32)(W - contentW) / 2;
    const irr::f32 cx1 = cx0 + contentW;
    const irr::f32 Hf = (irr::f32)H;
    const irr::f32 margin = 64 * k;
    const irr::f32 left = cx0 + margin;
    const irr::f32 right = cx1 - margin;

    driver->setViewPort(irr::core::rect<irr::s32>(0, 0, W, H));
    driver->beginScene(irr::video::ECBF_COLOR | irr::video::ECBF_DEPTH, SColor(255, 4, 14, 26));

    //--- Background: navy gradient, the photo over the whole width, a veil ---------------------------
    driver->draw2DRectangle(irr::core::rect<irr::s32>(0, 0, W, H),
        SColor(255, 8, 30, 52), SColor(255, 4, 18, 34), SColor(255, 2, 10, 20), SColor(255, 4, 16, 30));
    if (background) {
        const irr::core::dimension2d<irr::u32> ts = background->getOriginalSize();
        if (ts.Width > 0 && ts.Height > 0) {
            //'cover' over the content column, so its logos stay on the middle screen
            const irr::f32 destAspect = (irr::f32)contentW / Hf;
            const irr::f32 texAspect = (irr::f32)ts.Width / (irr::f32)ts.Height;
            irr::core::rect<irr::s32> src(0, 0, ts.Width, ts.Height);
            if (texAspect > destAspect) {
                const irr::s32 w = (irr::s32)(ts.Height * destAspect);
                src.UpperLeftCorner.X = ((irr::s32)ts.Width - w) / 2;
                src.LowerRightCorner.X = src.UpperLeftCorner.X + w;
            }
            else {
                const irr::s32 h = (irr::s32)(ts.Width / destAspect);
                src.UpperLeftCorner.Y = ((irr::s32)ts.Height - h) / 2;
                src.LowerRightCorner.Y = src.UpperLeftCorner.Y + h;
            }
            const SColor tint(255, 150, 160, 170);
            const SColor tints[4] = { tint, tint, tint, tint };
            driver->draw2DImage(background, irr::core::rect<irr::s32>((irr::s32)cx0, 0, (irr::s32)cx1, H), src, 0, tints, false);
        }
    }
    //Veil: light over the top band (the photo's logos), dark below where the cards are
    const irr::s32 band = (irr::s32)(Hf * 0.235f);
    driver->draw2DRectangle(irr::core::rect<irr::s32>(0, 0, W, band),
        SColor(70, 2, 10, 20), SColor(70, 2, 10, 20), SColor(150, 2, 10, 20), SColor(150, 2, 10, 20));
    //(dark enough at once that the photo's own big title, just under its logos, does not show through)
    const irr::s32 band2 = band + (irr::s32)(Hf * 0.035f);
    driver->draw2DRectangle(irr::core::rect<irr::s32>(0, band, W, band2),
        SColor(170, 2, 10, 20), SColor(170, 2, 10, 20), SColor(236, 2, 9, 18), SColor(236, 2, 9, 18));
    driver->draw2DRectangle(irr::core::rect<irr::s32>(0, band2, W, H),
        SColor(236, 2, 9, 18), SColor(236, 2, 9, 18), SColor(242, 2, 8, 16), SColor(242, 2, 8, 16));

    //Chart graticule, very faint, over the lower part
    {
        irr::gui::PanelBatch b;
        b.begin(driver);
        const irr::f32 step = 120 * k;
        for (irr::f32 x = cx0 + fmodf(contentW * 0.5f, step); x < cx1; x += step) {
            b.rectV(RectF(x, (irr::f32)band, x + 1, Hf), alpha(COL_ACCENT, 26), alpha(COL_ACCENT, 6));
        }
        for (irr::f32 y = (irr::f32)band + step * 0.5f; y < Hf; y += step) {
            b.rect(RectF(cx0, y, cx1, y + 1), alpha(COL_ACCENT, 14));
        }
        b.flush();
    }

    //Without a photo, the name of the product where its logos would be
    if (!background) {
        fonts.kicker->drawIn(L"NAUTITECH  \u00B7  SIMULATEUR DE NAVIGATION MARITIME", RectF(left, 40 * k, right, 70 * k), COL_MUTED, HudFont::Left, 4 * k);
        driver->draw2DRectangle(COL_ACCENT, toI(RectF(left, 78 * k, left + 90 * k, 81 * k)));
    }

    //--- Layout -----------------------------------------------------------------------------------------
    const irr::f32 top = Hf * 0.255f;
    const irr::f32 factTop = Hf * 0.775f;
    const irr::f32 mainBottom = factTop - 26 * k;
    const irr::f32 scopeColW = (right - left) * 0.33f;
    const irr::f32 gap = 44 * k;
    const RectF card(left, top, right - scopeColW - gap, mainBottom);
    drawExerciseCard(card);

    //Radar scope, and the stages under it
    {
        const irr::f32 colX0 = right - scopeColW;
        const irr::f32 stagesH = 4 * 34 * k;
        irr::f32 R = std::min(scopeColW * 0.40f, (mainBottom - top - stagesH - 70 * k) * 0.5f);
        R = std::max(R, 40 * k);
        const irr::f32 cx = colX0 + scopeColW * 0.5f;
        const irr::f32 cy = top + 34 * k + R;
        drawScope(cx, cy, R);
        drawStages(colX0 + scopeColW * 0.06f, cy + R + 36 * k, scopeColW * 0.88f);
    }

    drawFactCard(RectF(left, factTop, right, Hf * 0.925f));

    //--- Progress line and footer --------------------------------------------------------------------------
    {
        const irr::f32 y = Hf - 40 * k;
        driver->draw2DRectangle(alpha(COL_ACCENT, 50), toI(RectF(left, y, right, y + 3 * k)));
        driver->draw2DRectangle(toI(RectF(left, y, left + (right - left) * progress, y + 3 * k)),
            COL_ACCENT, COL_ACCENT_HI, COL_ACCENT, COL_ACCENT_HI);
        fonts.small->drawIn(L"NAUTITECH S.A.R.L.  \u00B7  VOTRE PARTENAIRE EN TECHNOLOGIES MARITIMES ET SYST\u00C8MES DE NAVIGATION",
            RectF(left, y + 8 * k, right, Hf - 6 * k), COL_DIM, HudFont::Left, 1.5f * k);
        fonts.small->drawIn(L"www.nautitech.org", RectF(left, y + 8 * k, right, Hf - 6 * k), COL_DIM, HudFont::Right, 1.5f * k);
    }

    driver->endScene();
}

void LoadingScreen::drawExerciseCard(const RectF& card)
{
    const irr::core::rect<irr::s32> clip = toI(card);
    //Glass panel with a cyan edge
    bridge::fillRound(driver, toI(card), (irr::s32)(14 * k), SColor(215, 9, 28, 46), SColor(225, 4, 14, 26), 0);
    driver->draw2DRectangle(COL_ACCENT, toI(RectF(card.UpperLeftCorner.X, card.UpperLeftCorner.Y + 18 * k,
        card.UpperLeftCorner.X + 5 * k, card.UpperLeftCorner.Y + 110 * k)));

    const irr::f32 pad = 40 * k;
    const irr::f32 x0 = card.UpperLeftCorner.X + pad;
    const irr::f32 x1 = card.LowerRightCorner.X - pad;
    const irr::f32 innerW = x1 - x0;
    irr::f32 y = card.UpperLeftCorner.Y + 30 * k;

    fonts.kicker->draw(L"EXERCICE", x0, y, COL_ACCENT, 5 * k, &clip);
    y += 36 * k;

    //Title: big; smaller when it would take more than two lines
    const std::wstring shownTitle = hudUpper(title.empty() ? std::wstring(L"Chargement de l'exercice") : title);
    HudFont* tf = fonts.title;
    std::vector<std::wstring> lines = tf->wrap(shownTitle, innerW, 1.0f * k);
    if (lines.size() > 2) {
        tf = fonts.titleSmall;
        lines = tf->wrap(shownTitle, innerW, 1.0f * k);
        if (lines.size() > 2) { lines.resize(2); lines[1] += L"\u2026"; }
    }
    for (size_t i = 0; i < lines.size(); i++) {
        tf->draw(lines[i], x0, y, COL_TEXT, 1.0f * k, &clip);
        y += tf->lineHeight() * 0.92f;
    }
    y += 10 * k;
    driver->draw2DRectangle(toI(RectF(x0, y, x0 + 120 * k, y + 4 * k)), COL_ACCENT, alpha(COL_ACCENT, 0), COL_ACCENT, alpha(COL_ACCENT, 0));
    y += 30 * k;

    //Conditions: one tile each, four to a row
    if (!infoLabels.empty()) {
        const size_t n = infoLabels.size();
        const size_t cols = std::min<size_t>(4, n);
        const irr::f32 tg = 12 * k;
        const irr::f32 tw = (innerW - (cols - 1) * tg) / cols;
        const irr::f32 th = 82 * k;
        const size_t rows = (n + cols - 1) / cols;
        for (size_t i = 0; i < n; i++) {
            const irr::f32 tx = x0 + (i % cols) * (tw + tg);
            const irr::f32 ty = y + (i / cols) * (th + tg);
            const RectF tile(tx, ty, tx + tw, ty + th);
            bridge::fillRound(driver, toI(tile), (irr::s32)(8 * k), SColor(120, 18, 48, 74), SColor(120, 8, 26, 44), &clip);
            driver->draw2DRectangle(alpha(COL_ACCENT, 140), toI(RectF(tx, ty + th - 3 * k, tx + tw, ty + th)), &clip);
            irr::core::rect<irr::s32> tileClip = toI(RectF(tx + 4 * k, ty, tx + tw - 4 * k, ty + th));
            tileClip.clipAgainst(clip);
            fonts.kicker->drawIn(hudUpper(infoLabels[i]), RectF(tx + 16 * k, ty + 8 * k, tx + tw - 8 * k, ty + 34 * k), COL_MUTED, HudFont::Left, 2.5f * k, &tileClip);
            fonts.value->drawIn(infoValues[i], RectF(tx + 16 * k, ty + 36 * k, tx + tw - 8 * k, ty + th - 10 * k), COL_TEXT, HudFont::Left, 0.5f * k, &tileClip);
        }
        y += rows * (th + tg) + 18 * k;
    }

    //Briefing, as much as fits
    if (!briefing.empty()) {
        const irr::f32 lh = fonts.body->lineHeight() * 1.18f;
        const irr::f32 bottom = card.LowerRightCorner.Y - pad * 0.7f;
        if (y + 30 * k + lh <= bottom) {
            fonts.kicker->draw(L"BRIEFING", x0, y, COL_ACCENT, 5 * k, &clip);
            y += 34 * k;
            std::vector<std::wstring> text;
            //paragraph by paragraph
            size_t pos = 0;
            while (pos <= briefing.size()) {
                const size_t nl = briefing.find(L'\n', pos);
                const std::wstring para = briefing.substr(pos, nl == std::wstring::npos ? std::wstring::npos : nl - pos);
                const std::vector<std::wstring> w = fonts.body->wrap(para, innerW);
                text.insert(text.end(), w.begin(), w.end());
                if (nl == std::wstring::npos) { break; }
                pos = nl + 1;
            }
            const size_t fit = (size_t)std::max(0.0f, floorf((bottom - y) / lh));
            if (text.size() > fit && fit > 0) { text.resize(fit); text[fit - 1] += L" \u2026"; }
            for (size_t i = 0; i < text.size() && i < fit; i++) {
                fonts.body->draw(text[i], x0, y, COL_SOFT, 0, &clip);
                y += lh;
            }
        }
    }
}

void LoadingScreen::drawScope(irr::f32 cx, irr::f32 cy, irr::f32 R)
{
    const vector2df c(cx, cy);
    const irr::f32 sweep = progress * 360.0f;
    irr::gui::PanelBatch b;
    b.begin(driver);
    //halo, bezel, screen
    b.sector(c, R, R + 30 * k, 0, 360, alpha(COL_ACCENT, 46), alpha(COL_ACCENT, 0), false);
    b.sector(c, R, R + 5 * k, 0, 360, SColor(255, 34, 70, 96), SColor(255, 14, 32, 50));
    b.disc(c, R, SColor(240, 6, 26, 40), SColor(240, 2, 12, 22));
    //what has been swept, brighter towards the rim, and the trail behind the sweep line
    if (sweep > 0.5f) {
        b.sector(c, 0, R, 0, sweep, alpha(COL_ACCENT, 10), alpha(COL_ACCENT, 80), false);
        b.sector(c, 0, R, std::max(0.0f, sweep - 32.0f), sweep, alpha(COL_ACCENT_HI, 20), alpha(COL_ACCENT_HI, 150), false);
    }
    //range rings and cross
    for (int i = 1; i <= 3; i++) {
        const irr::f32 r = R * i / 4.0f;
        b.sector(c, r - 0.7f * k, r + 0.7f * k, 0, 360, alpha(COL_ACCENT, 70), alpha(COL_ACCENT, 70), false);
    }
    b.line(vector2df(cx - R, cy), vector2df(cx + R, cy), 1.0f * k, alpha(COL_ACCENT, 45));
    b.line(vector2df(cx, cy - R), vector2df(cx, cy + R), 1.0f * k, alpha(COL_ACCENT, 45));
    //bearing ticks round the rim
    for (int d = 0; d < 360; d += 5) {
        const irr::f32 len = (d % 30 == 0) ? 16 * k : ((d % 10 == 0) ? 10 * k : 5 * k);
        const irr::u32 a = (d % 30 == 0) ? 230 : ((d % 10 == 0) ? 150 : 80);
        b.line(polar(c, R - len, (irr::f32)d), polar(c, R, (irr::f32)d), (d % 30 == 0 ? 2.0f : 1.2f) * k, alpha(COL_ACCENT_HI, a));
    }
    //echoes: fixed places, lit once the sweep has passed them, brightest just after
    unsigned int seed = 12345u;
    for (int i = 0; i < 18; i++) {
        seed = seed * 1103515245u + 12345u;
        const irr::f32 ang = (irr::f32)((seed >> 8) % 3600) / 10.0f;
        seed = seed * 1103515245u + 12345u;
        const irr::f32 rr = R * (0.18f + 0.75f * (irr::f32)((seed >> 8) % 1000) / 1000.0f);
        if (ang > sweep) { continue; }
        const irr::f32 age = sweep - ang;
        const irr::u32 a = age < 40.0f ? 255 : 150;
        const irr::f32 sz = (age < 40.0f ? 4.5f : 3.2f) * k;
        b.disc(polar(c, rr, ang), sz, alpha(COL_ACCENT_HI, a), alpha(COL_ACCENT_HI, a / 3));
    }
    //sweep line
    if (sweep > 0.5f && sweep < 359.5f) {
        b.line(c, polar(c, R, sweep), 2.6f * k, SColor(255, 170, 240, 255));
    }
    //quiet centre for the figure
    b.disc(c, R * 0.42f, SColor(215, 3, 14, 26), SColor(150, 3, 14, 26));
    b.flush();

    //bearings
    for (int d = 0; d < 360; d += 30) {
        wchar_t t[8];
        swprintf(t, 8, L"%03d", d);
        const vector2df p = polar(c, R + 22 * k, (irr::f32)d);
        fonts.small->drawIn(t, RectF(p.X - 30 * k, p.Y - 12 * k, p.X + 30 * k, p.Y + 12 * k), COL_MUTED, HudFont::Centre, 1.0f * k);
    }
    //the figure
    wchar_t pct[16];
    swprintf(pct, 16, L"%d%%", (int)(progress * 100.0f + 0.5f));
    fonts.hero->drawIn(pct, RectF(cx - R, cy - R * 0.36f, cx + R, cy + R * 0.12f), COL_TEXT, HudFont::Centre, 0);
    fonts.kicker->drawIn(L"CHARGEMENT", RectF(cx - R, cy + R * 0.14f, cx + R, cy + R * 0.30f), COL_ACCENT, HudFont::Centre, 4 * k);

    //HUD corners round the scope
    {
        const irr::f32 e = R + 34 * k, l = 26 * k, w = 2.0f * k;
        irr::gui::PanelBatch c2;
        c2.begin(driver);
        const SColor col = alpha(COL_ACCENT, 160);
        for (int sx = -1; sx <= 1; sx += 2) {
            for (int sy = -1; sy <= 1; sy += 2) {
                const vector2df corner(cx + sx * e, cy + sy * e);
                c2.line(corner, vector2df(corner.X - sx * l, corner.Y), w, col);
                c2.line(corner, vector2df(corner.X, corner.Y - sy * l), w, col);
            }
        }
        c2.flush();
    }
}

void LoadingScreen::drawStages(irr::f32 x, irr::f32 y, irr::f32 width)
{
    //The last stages done (ticked), then the one under way
    const irr::f32 rowH = 34 * k;
    const size_t shown = std::min<size_t>(3, doneStages.size());
    for (size_t i = doneStages.size() - shown; i < doneStages.size(); i++) {
        drawCheck(driver, x, y + 6 * k, 16 * k, alpha(COL_ACCENT, 200));
        fonts.body->drawIn(doneStages[i], RectF(x + 30 * k, y, x + width, y + rowH * 0.8f), COL_MUTED, HudFont::Left, 0);
        y += rowH;
    }
    if (!stageText.empty()) {
        irr::gui::PanelBatch b;
        b.begin(driver);
        b.disc(vector2df(x + 8 * k, y + rowH * 0.42f), 9 * k, alpha(COL_ACCENT_HI, 70), alpha(COL_ACCENT_HI, 0));
        b.disc(vector2df(x + 8 * k, y + rowH * 0.42f), 5 * k, COL_ACCENT_HI, COL_ACCENT_HI);
        b.flush();
        fonts.stage->drawIn(stageText, RectF(x + 30 * k, y, x + width, y + rowH * 0.84f), COL_TEXT, HudFont::Left, 0.5f * k);
    }
}

void LoadingScreen::drawFactCard(const RectF& card)
{
    if (facts.empty()) { return; }
    const irr::core::rect<irr::s32> clip = toI(card);
    bridge::fillRound(driver, toI(card), (irr::s32)(14 * k), SColor(185, 6, 22, 38), SColor(215, 3, 12, 22), 0);

    //Lighthouse on a cyan block
    const irr::f32 iconW = card.getHeight();
    const RectF icon(card.UpperLeftCorner.X, card.UpperLeftCorner.Y, card.UpperLeftCorner.X + iconW, card.LowerRightCorner.Y);
    bridge::fillRound(driver, toI(icon), (irr::s32)(14 * k), SColor(255, 0, 140, 184), SColor(255, 0, 96, 140), 0);
    const irr::f32 s = iconW * 0.72f;
    drawLighthouse(driver, icon.getCenter().X - s * 0.5f, icon.getCenter().Y - s * 0.52f, s, SColor(255, 240, 248, 252), SColor(255, 0, 96, 140));

    const irr::f32 x0 = icon.LowerRightCorner.X + 30 * k;
    const irr::f32 x1 = card.LowerRightCorner.X - 34 * k;
    irr::f32 y = card.UpperLeftCorner.Y + 20 * k;
    fonts.kicker->draw(L"LE SAVIEZ-VOUS ?", x0, y, COL_ACCENT, 5 * k, &clip);
    wchar_t counter[24];
    swprintf(counter, 24, L"%d / %d", (int)factIndex + 1, (int)facts.size());
    fonts.kicker->drawIn(counter, RectF(x0, y - 4 * k, x1, y + 22 * k), COL_DIM, HudFont::Right, 2 * k, &clip);
    y += 38 * k;

    //Two lines in the big size, else three in the smaller one
    const std::wstring& fact = facts[factIndex % facts.size()];
    HudFont* f = fonts.fact;
    std::vector<std::wstring> lines = f->wrap(fact, x1 - x0);
    if (lines.size() > 2) {
        f = fonts.factSmall;
        lines = f->wrap(fact, x1 - x0);
    }
    if (lines.size() > 3) { lines.resize(3); lines[2] += L"\u2026"; }
    //centred in the room under the heading
    const irr::f32 lh = f->lineHeight() * 1.06f;
    const irr::f32 room = card.LowerRightCorner.Y - 14 * k - y;
    y += std::max(0.0f, (room - lh * lines.size()) * 0.5f);
    for (size_t i = 0; i < lines.size() && y + lh * 0.8f <= card.LowerRightCorner.Y; i++) {
        f->draw(lines[i], x0, y, COL_TEXT, 0, &clip);
        y += lh;
    }
}

std::wstring LoadingScreen::fromUtf8(const std::string& s)
{
    //Decodes UTF-8. Any byte sequence that is not valid UTF-8 is taken as Windows-1252, so a
    //description.ini or a facts file saved as ANSI still shows its accents and apostrophes.
    static const unsigned short CP1252[32] = {
        0x20AC, 0x003F, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x003F, 0x017D, 0x003F,
        0x003F, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x003F, 0x017E, 0x0178 };
    std::wstring out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = (unsigned char)s[i];
        unsigned int cp = 0;
        size_t extra = 0;
        bool valid = true;
        if (c < 0x80) { cp = c; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { valid = false; }

        if (valid && i + extra >= s.size()) { valid = false; }
        for (size_t k = 1; valid && k <= extra; k++) {
            unsigned char cc = (unsigned char)s[i + k];
            if ((cc & 0xC0) != 0x80) { valid = false; }
            else { cp = (cp << 6) | (cc & 0x3F); }
        }

        if (!valid) {
            cp = (c >= 0x80 && c < 0xA0) ? CP1252[c - 0x80] : c; //Windows-1252 for this byte
            extra = 0;
        }
        i += extra + 1;

        if (cp == L'\r') { continue; }
        if (cp > 0xFFFF && sizeof(wchar_t) == 2) { cp = L'?'; } //outside the BMP: not in our fonts anyway
        out += (wchar_t)cp;
    }
    return out;
}
