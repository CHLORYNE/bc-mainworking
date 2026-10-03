//Lettering for the full-screen launcher: TrueType faces rasterised at the size the screen needs
//(stb_truetype), so that titles stay sharp from 720p to 4K. One HudFont is one face at one pixel size.
//If the TrueType file cannot be read, the HudFont draws with the Irrlicht bitmap font given instead.

#ifndef __LAUNCHER_HUDTEXT_HPP_INCLUDED__
#define __LAUNCHER_HUDTEXT_HPP_INCLUDED__

#include "irrlicht.h"
#include <string>
#include <vector>
#include <map>

class HudFont
{
public:
    //file: TrueType file. pixels: height from the top of the tallest letters to the bottom of the
    //descenders. fallback: bitmap font used if the file cannot be read (may be 0).
    HudFont(irr::video::IVideoDriver* driver, const std::string& file, irr::f32 pixels, irr::gui::IGUIFont* fallback);
    ~HudFont();

    bool isTrueType() const { return texture != 0; }

    //Width of the text; tracking is extra space after every letter, in pixels.
    irr::f32 width(const std::wstring& text, irr::f32 tracking = 0) const;
    //Height of capital letters, and of the whole line (ascender to descender).
    irr::f32 capHeight() const { return caps; }
    irr::f32 lineHeight() const { return ascent - descent; }

    //Draws the text with the top of its capital letters at y. Returns the width drawn.
    irr::f32 draw(const std::wstring& text, irr::f32 x, irr::f32 y, irr::video::SColor col, irr::f32 tracking = 0,
        const irr::core::rect<irr::s32>* clip = 0) const;

    enum Align { Left, Centre, Right };
    //Text centred vertically (on its capitals) in the box, aligned horizontally.
    void drawIn(const std::wstring& text, const irr::core::rect<irr::f32>& box, irr::video::SColor col, Align align = Left,
        irr::f32 tracking = 0, const irr::core::rect<irr::s32>* clip = 0) const;

    //Word-wrapped lines for the width.
    std::vector<std::wstring> wrap(const std::wstring& text, irr::f32 maxWidth, irr::f32 tracking = 0) const;

private:
    struct Glyph {
        irr::core::rect<irr::s32> source; //in the atlas
        irr::f32 left, top;               //bitmap offset from the pen on the baseline
        irr::f32 advance;
        int index;                        //glyph index in the face, for kerning
    };
    const Glyph* glyph(wchar_t c) const;
    irr::f32 kerning(int a, int b) const;

    irr::video::IVideoDriver* driver;
    irr::gui::IGUIFont* fallback;
    irr::video::ITexture* texture;
    std::map<wchar_t, Glyph> glyphs;
    void* face;                           //stbtt_fontinfo
    irr::f32 scale, ascent, descent, caps;
};

//Upper case for HUD labels, accents included (Latin-1, oe ligature).
std::wstring hudUpper(const std::wstring& text);

#endif
