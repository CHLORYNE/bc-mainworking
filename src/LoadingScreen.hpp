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
//page cannot animate on its own: it is redrawn each time the loading code calls setStage(). The
//radar sweep therefore moves in steps - that is expected. Each stage's duration is written to the
//console/log ("[CHARGEMENT] ..."), which is how you find out what actually makes the loading slow.
//
//The page: the exercise on a card (title, conditions as tiles, briefing), the progress as a radar
//scope whose sweep fills clockwise with the stages done listed under it, and a "Le saviez-vous ?"
//card at the bottom. Lettering in Barlow Condensed (media/fonts/barlow-condensed), sized from the
//screen height.
//
//Files (all optional - the page works without them):
//   media/loading_bg.jpg       background photo, scaled to cover the screen and dimmed (its top band,
//                              where the logos are, is left lighter)
//   media/le_saviez_vous.txt   the facts, one per line "N. text" (UTF-8, or Windows ANSI)
class HudFont;

class LoadingScreen {
public:
    LoadingScreen(irr::IrrlichtDevice* device, irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* titleFont);
    ~LoadingScreen();

    //Scenario card. All text is UTF-8 (accents OK), converted internally.
    void setTitle(const std::string& titleUtf8);
    void addInfo(const std::string& labelUtf8, const std::string& valueUtf8);
    void setBriefing(const std::string& textUtf8);

    //Start a new loading stage: logs how long the previous stage took, then redraws at once.
    //progress is 0..1.
    void setStage(irr::f32 progress, const std::string& stageTextUtf8);

    //Redraw at most every 100 ms - for wait loops (secondary waiting for the primary).
    void refresh();

    //Last stage done: shows 100 %, logs the total, frees the background and the fonts.
    void finish();

    //UTF-8 to wide; a byte that is not valid UTF-8 is read as Windows-1252 (so an ANSI file keeps
    //its accents and typographic apostrophes).
    static std::wstring fromUtf8(const std::string& s);

private:
    void draw();
    void drawScope(irr::f32 cx, irr::f32 cy, irr::f32 radius);
    void drawExerciseCard(const irr::core::rect<irr::f32>& card);
    void drawFactCard(const irr::core::rect<irr::f32>& card);
    void drawStages(irr::f32 x, irr::f32 y, irr::f32 width);
    void loadFacts();
    void loadFonts(irr::f32 scale);
    void freeFonts();
    void logPreviousStage();
    irr::u32 now() const;

    irr::IrrlichtDevice* device;
    irr::video::IVideoDriver* driver;
    irr::gui::IGUIFont* textFont;
    irr::gui::IGUIFont* titleFont;
    irr::video::ITexture* background;

    std::wstring title;
    std::vector<std::wstring> infoLabels;
    std::vector<std::wstring> infoValues;
    std::wstring briefing;

    std::vector<std::wstring> facts;
    size_t factIndex;
    irr::u32 factShownTime;

    irr::f32 progress;
    std::wstring stageText;
    std::vector<std::wstring> doneStages;
    std::string stageLogName;
    irr::u32 stageStartTime;
    irr::u32 loadStartTime;
    irr::u32 lastDrawTime;

    irr::f32 k;                      //scale: screen height / 1080
    struct Fonts
    {
        HudFont* hero = 0;           //the percentage
        HudFont* title = 0;          //exercise name
        HudFont* titleSmall = 0;     //exercise name when long
        HudFont* value = 0;          //tile values
        HudFont* body = 0;           //briefing
        HudFont* fact = 0;           //"le saviez-vous" text
        HudFont* factSmall = 0;      //...when long
        HudFont* stage = 0;          //current stage
        HudFont* kicker = 0;         //small capitals
        HudFont* small = 0;          //scope bearings, footer
    } fonts;
};

#endif
