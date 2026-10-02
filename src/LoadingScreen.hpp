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

#ifndef __LOADINGSCREEN_HPP_INCLUDED__
#define __LOADINGSCREEN_HPP_INCLUDED__

#include "irrlicht.h"
#include <string>
#include <vector>

//KYARA CHARGEMENT: full-screen loading page, drawn directly with the video driver (not the GUI
//environment, so the simulator GUI that is being built behind it never shows through).
//
//Irrlicht/OpenGL is single-threaded and the loading itself uses the driver (texture uploads), so the
//page cannot animate on its own: it is redrawn each time the loading code calls setStage(). The bar
//therefore moves in steps - that is expected. Each stage's duration is written to the console/log
//("[CHARGEMENT] ..."), which is how you find out what actually makes the loading slow.
//
//Optional images (the page works without them - it falls back to a dark navy gradient):
//   media/loading_bg.jpg       background photo, scaled to cover the screen and dimmed
//   media/logo_nautitech.png   logo, top-left, PNG with transparency
class LoadingScreen {
public:
    LoadingScreen(irr::IrrlichtDevice* device, irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* titleFont);

    //Scenario card. All text is UTF-8 (accents OK), converted internally.
    void setTitle(const std::string& titleUtf8);
    void addInfo(const std::string& labelUtf8, const std::string& valueUtf8);
    void setBriefing(const std::string& textUtf8);

    //Start a new loading stage: logs how long the previous stage took, then redraws at once.
    //progress is 0..1.
    void setStage(irr::f32 progress, const std::string& stageTextUtf8);

    //Redraw at most every 100 ms - for wait loops (secondary waiting for the primary).
    void refresh();

    //Last stage done: shows 100 %, logs the total, frees the background texture.
    void finish();

    static std::wstring fromUtf8(const std::string& s);

private:
    void draw();
    void logPreviousStage();
    std::vector<std::wstring> wrapText(irr::gui::IGUIFont* f, const std::wstring& text, irr::s32 maxWidth, size_t maxLines) const;
    void drawText(irr::gui::IGUIFont* f, const std::wstring& text, irr::s32 x, irr::s32 y, irr::video::SColor colour) const;
    irr::u32 now() const;

    irr::IrrlichtDevice* device;
    irr::video::IVideoDriver* driver;
    irr::gui::IGUIFont* textFont;
    irr::gui::IGUIFont* titleFont;
    irr::video::ITexture* background;
    irr::video::ITexture* logo;

    std::wstring title;
    std::vector<std::wstring> infoLabels;
    std::vector<std::wstring> infoValues;
    std::wstring briefing;
    std::wstring tip;

    irr::f32 progress;
    std::wstring stageText;
    std::string stageLogName;
    irr::u32 stageStartTime;
    irr::u32 loadStartTime;
    irr::u32 lastDrawTime;
};

#endif
