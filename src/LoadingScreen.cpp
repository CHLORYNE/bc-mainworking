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

//NOTE: this file is saved as UTF-8. The French strings are narrow UTF-8 literals decoded by
//fromUtf8(), so the accents survive whatever source charset MSVC assumes.

#include "LoadingScreen.hpp"
#include <iostream>
#include <sstream>

namespace {
    //KYARA CHARGEMENT: palette
    const irr::video::SColor COL_ACCENT(255, 0, 168, 214);   //maritime cyan
    const irr::video::SColor COL_ACCENT_HI(255, 90, 214, 245);
    const irr::video::SColor COL_TEXT(255, 236, 242, 248);
    const irr::video::SColor COL_MUTED(255, 138, 160, 182);
    const irr::video::SColor COL_DIM(255, 88, 108, 128);
    const irr::video::SColor COL_TRACK(255, 26, 44, 64);

    //One tip is picked at random per load. Keep them short - two lines at most on screen.
    const char* const TIPS[] = {
        "Règle 5 : tout navire doit assurer en permanence une veille visuelle et auditive appropriée.",
        "Règle 6 : la vitesse de sécurité dépend notamment de la visibilité, de la densité du trafic et de la capacité de manoeuvre du navire.",
        "Règle 7 : si le relèvement d'un navire qui s'approche ne varie pas de façon appréciable, il existe un risque d'abordage.",
        "Règle 8 : une manoeuvre d'évitement doit être franche, exécutée largement à temps et nettement perceptible par l'autre navire.",
        "Règle 13 : tout navire qui en rattrape un autre doit s'écarter de la route de ce dernier.",
        "Règle 14 : deux navires à propulsion mécanique à contre-bord viennent chacun sur tribord.",
        "Règle 15 : en routes croisées, le navire qui voit l'autre sur son tribord doit s'écarter de sa route.",
        "Règle 19 : par visibilité réduite, un écho détecté au radar seul impose d'évaluer le risque de situation très rapprochée.",
        "Radar : surveillez le CPA et le TCPA de chaque cible - un relèvement constant avec une distance qui diminue est un signal d'alerte.",
    };
    const size_t TIP_COUNT = sizeof(TIPS) / sizeof(TIPS[0]);

    irr::video::ITexture* loadOptionalTexture(irr::IrrlichtDevice* device, const char* path)
    {
        //existFile first: getTexture() on a missing file would print an error in the log
        if (device->getFileSystem()->existFile(path)) {
            return device->getVideoDriver()->getTexture(path);
        }
        return 0;
    }
}

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
    logo = 0;
    if (device) {
        background = loadOptionalTexture(device, "media/loading_bg.jpg");
        if (!background) { background = loadOptionalTexture(device, "media/loading_bg.png"); }
        logo = loadOptionalTexture(device, "media/logo_nautitech.png");
    }

    progress = 0.0f;
    stageStartTime = now();
    loadStartTime = stageStartTime;
    lastDrawTime = 0;

    tip = fromUtf8(TIPS[now() % TIP_COUNT]);
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
    stageText = fromUtf8(stageTextUtf8);
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
    stageText = fromUtf8("Prêt");
    draw();

    std::ostringstream line;
    line << "[CHARGEMENT] TOTAL : " << (now() - loadStartTime) << " ms";
    std::cout << line.str() << std::endl;
    if (device) { device->getLogger()->log(line.str().c_str()); }

    //The background photo is only used here - give the VRAM back to the simulation.
    if (background && driver) {
        driver->removeTexture(background);
        background = 0;
    }
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

void LoadingScreen::drawText(irr::gui::IGUIFont* f, const std::wstring& text, irr::s32 x, irr::s32 y, irr::video::SColor colour) const
{
    if (!f || text.empty()) { return; }
    irr::core::dimension2d<irr::u32> d = f->getDimension(text.c_str());
    f->draw(irr::core::stringw(text.c_str()), irr::core::rect<irr::s32>(x, y, x + (irr::s32)d.Width + 2, y + (irr::s32)d.Height + 2), colour);
}

std::vector<std::wstring> LoadingScreen::wrapText(irr::gui::IGUIFont* f, const std::wstring& text, irr::s32 maxWidth, size_t maxLines) const
{
    std::vector<std::wstring> lines;
    if (!f || text.empty() || maxWidth <= 0 || maxLines == 0) { return lines; }

    size_t pos = 0;
    while (pos < text.size() && lines.size() <= maxLines) {
        size_t nl = text.find(L'\n', pos);
        std::wstring para = text.substr(pos, (nl == std::wstring::npos) ? std::wstring::npos : nl - pos);

        std::wstring line;
        size_t i = 0;
        while (i < para.size()) {
            size_t sp = para.find(L' ', i);
            std::wstring word = para.substr(i, (sp == std::wstring::npos) ? std::wstring::npos : sp - i);
            i = (sp == std::wstring::npos) ? para.size() : sp + 1;
            if (word.empty()) { continue; }
            std::wstring candidate = line.empty() ? word : line + L" " + word;
            if (!line.empty() && (irr::s32)f->getDimension(candidate.c_str()).Width > maxWidth) {
                lines.push_back(line);
                line = word;
            }
            else {
                line = candidate;
            }
        }
        if (!line.empty()) { lines.push_back(line); }

        if (nl == std::wstring::npos) { break; }
        pos = nl + 1;
    }

    if (lines.size() > maxLines) {
        lines.resize(maxLines);
        lines.back() += L" ...";
    }
    return lines;
}

void LoadingScreen::draw()
{
    if (!device || !driver) { return; }

    //Pump the window messages. Without this, Windows marks the window "Ne répond pas" during a long load.
    device->run();
    lastDrawTime = now();

    const irr::core::dimension2d<irr::u32> ss = driver->getScreenSize();
    const irr::s32 W = (irr::s32)ss.Width;
    const irr::s32 H = (irr::s32)ss.Height;
    if (W <= 0 || H <= 0) { return; }

    //Content column: never wider than 16:9 of the height, centred. On the triple-screen (Eyefinity)
    //surface this puts everything on the centre TV, and the side TVs get the plain gradient.
    irr::s32 contentW = W;
    if (contentW > H * 16 / 9) { contentW = H * 16 / 9; }
    const irr::s32 cx0 = (W - contentW) / 2;
    const irr::s32 cx1 = cx0 + contentW;
    const irr::s32 margin = contentW / 16;
    const irr::s32 left = cx0 + margin;
    const irr::s32 right = cx1 - margin;
    const irr::s32 textH = (irr::s32)textFont->getDimension(L"Ag").Height;
    const irr::s32 titleH = (irr::s32)titleFont->getDimension(L"Ag").Height;

    driver->setViewPort(irr::core::rect<irr::s32>(0, 0, W, H));
    driver->beginScene(irr::video::ECBF_COLOR | irr::video::ECBF_DEPTH, irr::video::SColor(255, 4, 14, 26));

    //--- Background: navy gradient over everything -------------------------------------------------
    driver->draw2DRectangle(irr::core::rect<irr::s32>(0, 0, W, H),
        irr::video::SColor(255, 8, 30, 52), irr::video::SColor(255, 4, 18, 34),
        irr::video::SColor(255, 2, 10, 20), irr::video::SColor(255, 4, 16, 30));

    //--- Optional photo, 'cover' scaled into the content column, dimmed ------------------------------
    if (background) {
        const irr::core::dimension2d<irr::u32> ts = background->getOriginalSize();
        if (ts.Width > 0 && ts.Height > 0) {
            const irr::f32 destAspect = (irr::f32)contentW / (irr::f32)H;
            const irr::f32 texAspect = (irr::f32)ts.Width / (irr::f32)ts.Height;
            irr::core::rect<irr::s32> src(0, 0, ts.Width, ts.Height);
            if (texAspect > destAspect) { //texture too wide: crop the sides
                irr::s32 w = (irr::s32)(ts.Height * destAspect);
                src.UpperLeftCorner.X = ((irr::s32)ts.Width - w) / 2;
                src.LowerRightCorner.X = src.UpperLeftCorner.X + w;
            }
            else { //texture too tall: crop top and bottom
                irr::s32 h = (irr::s32)(ts.Width / destAspect);
                src.UpperLeftCorner.Y = ((irr::s32)ts.Height - h) / 2;
                src.LowerRightCorner.Y = src.UpperLeftCorner.Y + h;
            }
            const irr::video::SColor dim(255, 125, 135, 145);
            const irr::video::SColor dims[4] = { dim, dim, dim, dim };
            driver->draw2DImage(background, irr::core::rect<irr::s32>(cx0, 0, cx1, H), src, 0, dims, false);
        }
    }

    //Readability veil: dark on the left where the text sits, lighter on the right
    driver->draw2DRectangle(irr::core::rect<irr::s32>(cx0, 0, cx1, H),
        irr::video::SColor(225, 3, 12, 22), irr::video::SColor(70, 3, 12, 22),
        irr::video::SColor(240, 3, 12, 22), irr::video::SColor(150, 3, 12, 22));
    //Darker band at the bottom for the progress bar
    driver->draw2DRectangle(irr::core::rect<irr::s32>(0, H * 3 / 4, W, H),
        irr::video::SColor(0, 2, 8, 16), irr::video::SColor(0, 2, 8, 16),
        irr::video::SColor(220, 2, 8, 16), irr::video::SColor(220, 2, 8, 16));

    //--- Header: logo left, product name right --------------------------------------------------------
    const irr::s32 topY = H / 16;
    if (logo) {
        const irr::core::dimension2d<irr::u32> ls = logo->getOriginalSize();
        if (ls.Width > 0 && ls.Height > 0) {
            irr::s32 lh = H / 12;
            irr::s32 lw = (irr::s32)((irr::f32)ls.Width * lh / ls.Height);
            driver->draw2DImage(logo, irr::core::rect<irr::s32>(left, topY, left + lw, topY + lh),
                irr::core::rect<irr::s32>(0, 0, ls.Width, ls.Height), 0, 0, true);
        }
    }
    {
        std::wstring product = fromUtf8("SIMULATEUR DE NAVIGATION MARITIME");
        irr::s32 w = (irr::s32)textFont->getDimension(product.c_str()).Width;
        drawText(textFont, product, right - w, topY, COL_MUTED);
        driver->draw2DRectangle(COL_ACCENT, irr::core::rect<irr::s32>(right - w, topY + textH + 4, right, topY + textH + 6));
    }

    //--- Scenario card ----------------------------------------------------------------------------------
    irr::s32 y = H * 27 / 100;
    const irr::s32 blockW = (right - left) * 7 / 10;

    drawText(textFont, fromUtf8("EXERCICE"), left, y, COL_ACCENT);
    y += textH * 3 / 2;

    std::wstring shownTitle = title.empty() ? fromUtf8("Chargement de l'exercice") : title;
    std::vector<std::wstring> titleLines = wrapText(titleFont, shownTitle, blockW, 2);
    for (size_t i = 0; i < titleLines.size(); i++) {
        drawText(titleFont, titleLines[i], left, y, COL_TEXT);
        y += titleH;
    }
    y += textH / 2;
    driver->draw2DRectangle(COL_ACCENT, irr::core::rect<irr::s32>(left, y, left + contentW / 20, y + 3));
    y += textH + 3;

    //Info grid: label (muted) + value (white). Two columns when there are more than 4 entries.
    if (!infoLabels.empty()) {
        irr::s32 labelW = 0;
        for (size_t i = 0; i < infoLabels.size(); i++) {
            irr::s32 w = (irr::s32)textFont->getDimension(infoLabels[i].c_str()).Width;
            if (w > labelW) { labelW = w; }
        }
        labelW += textH; //gap between label and value
        const size_t columns = (infoLabels.size() > 4) ? 2 : 1;
        const size_t rows = (infoLabels.size() + columns - 1) / columns;
        const irr::s32 colW = (right - left) / 2;
        const irr::s32 rowH = textH * 17 / 10;
        for (size_t i = 0; i < infoLabels.size(); i++) {
            size_t col = i / rows;
            size_t row = i % rows;
            irr::s32 x = left + (irr::s32)col * colW;
            irr::s32 ry = y + (irr::s32)row * rowH;
            drawText(textFont, infoLabels[i], x, ry, COL_MUTED);
            drawText(textFont, infoValues[i], x + labelW, ry, COL_TEXT);
        }
        y += (irr::s32)rows * rowH;
    }

    //Briefing (description.ini of the scenario), a few lines only
    const irr::s32 barY = H * 86 / 100;
    if (!briefing.empty()) {
        y += textH;
        drawText(textFont, fromUtf8("BRIEFING"), left, y, COL_ACCENT);
        y += textH * 3 / 2;
        //Stop well above the stage text of the progress bar
        irr::s32 room = (barY - textH * 3) - y;
        size_t maxLines = (room > 0) ? (size_t)(room / (textH * 6 / 5)) : 0;
        if (maxLines > 6) { maxLines = 6; }
        std::vector<std::wstring> lines = wrapText(textFont, briefing, (right - left) * 6 / 10, maxLines);
        for (size_t i = 0; i < lines.size(); i++) {
            drawText(textFont, lines[i], left, y, COL_MUTED);
            y += textH * 6 / 5;
        }
    }

    //--- Progress -----------------------------------------------------------------------------------------
    irr::s32 barH = H / 180;
    if (barH < 4) { barH = 4; }
    {
        drawText(textFont, stageText, left, barY - textH * 3 / 2, COL_TEXT);

        wchar_t pct[16];
        swprintf(pct, 16, L"%d %%", (int)(progress * 100.0f + 0.5f));
        irr::s32 w = (irr::s32)textFont->getDimension(pct).Width;
        drawText(textFont, pct, right - w, barY - textH * 3 / 2, COL_ACCENT_HI);

        driver->draw2DRectangle(COL_TRACK, irr::core::rect<irr::s32>(left, barY, right, barY + barH));
        irr::s32 fillX = left + (irr::s32)((right - left) * progress);
        if (fillX > left) {
            driver->draw2DRectangle(irr::core::rect<irr::s32>(left, barY, fillX, barY + barH),
                COL_ACCENT, COL_ACCENT_HI, COL_ACCENT, COL_ACCENT_HI);
        }
    }

    //--- Tip ------------------------------------------------------------------------------------------------
    {
        irr::s32 ty = barY + barH + textH;
        std::wstring label = fromUtf8("Le saviez-vous ?  ");
        irr::s32 lw = (irr::s32)textFont->getDimension(label.c_str()).Width;
        drawText(textFont, label, left, ty, COL_ACCENT);
        std::vector<std::wstring> lines = wrapText(textFont, tip, (right - left) - lw, 2);
        for (size_t i = 0; i < lines.size(); i++) {
            drawText(textFont, lines[i], left + lw, ty + (irr::s32)i * (textH * 6 / 5), COL_MUTED);
        }
    }

    //--- Footer ---------------------------------------------------------------------------------------------
    {
        irr::s32 fy = H - textH * 2;
        drawText(textFont, fromUtf8("NAUTITECH S.A.R.L. - VOTRE PARTENAIRE EN TECHNOLOGIES MARITIMES ET SYSTEMES DE NAVIGATION"), left, fy, COL_DIM);
        std::wstring engine = fromUtf8("SITE WEB : www.nautitech.org");
        irr::s32 w = (irr::s32)textFont->getDimension(engine.c_str()).Width;
        drawText(textFont, engine, right - w, fy, COL_DIM);
    }

    driver->endScene();
}

std::wstring LoadingScreen::fromUtf8(const std::string& s)
{
    //Decodes UTF-8. Any byte sequence that is not valid UTF-8 is taken as Latin-1, so a
    //description.ini saved as ANSI/Windows-1252 still shows its accents.
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
            cp = c; //Latin-1 fallback for this byte
            extra = 0;
        }
        i += extra + 1;

        if (cp == L'\r') { continue; }
        if (cp > 0xFFFF && sizeof(wchar_t) == 2) { cp = L'?'; } //outside the BMP: not in our fonts anyway
        out += (wchar_t)cp;
    }
    return out;
}
