/*   NAUTITECH - Simulateur de Navigation
     The simulator's own look: one flat, quiet style for every button, window, tab, slider and
     field, in three palettes - day, dusk and night - the way bridge displays switch their
     colours with the light (IEC 62288). Night keeps to dim amber so the watch keeps its night
     vision.

     Header only, like UiTheme.hpp: the skin wraps Irrlicht's own one (fonts, sizes, icons) and
     only redraws the panes.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __BRIDGESKIN_HPP_INCLUDED__
#define __BRIDGESKIN_HPP_INCLUDED__

#include "irrlicht.h"
#include <cmath>
#include <map>
#include <vector>

namespace bridge
{
    enum Mode { MODE_DAY = 0, MODE_DUSK = 1, MODE_NIGHT = 2, MODE_DIGITAL = 3, MODE_COUNT = 4 };

    //The palette in use, shared by the skin, the console, the engine levers and the command bar
    inline int& currentMode()
    {
        static int mode = MODE_DUSK;
        return mode;
    }

    //How strongly the lit instruments glow, 0 (none) to 100; 50 is the normal backlight
    inline int& glowLevel()
    {
        static int level = 50;
        return level;
    }

    struct Palette
    {
        irr::video::SColor band;                         //behind the whole 2D area
        irr::video::SColor panelTop, panelBottom, edge;  //panels, windows, the command bar
        irr::video::SColor raised;                       //title bars, the tab strip
        irr::video::SColor key, keyHover;                //buttons
        irr::video::SColor text, textDim, textFaint;
        irr::video::SColor accent, accentText;           //selection, slider fill, "on" marks
        irr::video::SColor field;                        //edit boxes, lists, slider tracks
        irr::video::SColor danger, dangerText;
        irr::video::SColor ok, error;                    //status messages
        irr::video::SColor portText, stbdText;           //engine labels
    };

    inline const Palette& palette(int mode = -1)
    {
        using irr::video::SColor;
        static Palette p[MODE_COUNT];
        static bool made = false;
        if (!made) {
            made = true;
            //Day: light panels and dark text, readable in sunlight
            p[MODE_DAY].band = SColor(255, 174, 184, 195);
            p[MODE_DAY].panelTop = SColor(255, 227, 232, 238);
            p[MODE_DAY].panelBottom = SColor(255, 211, 218, 226);
            p[MODE_DAY].edge = SColor(255, 160, 172, 186);
            p[MODE_DAY].raised = SColor(255, 236, 240, 244);
            p[MODE_DAY].key = SColor(255, 238, 241, 245);
            p[MODE_DAY].keyHover = SColor(255, 255, 255, 255);
            p[MODE_DAY].text = SColor(255, 24, 34, 46);
            p[MODE_DAY].textDim = SColor(255, 84, 97, 112);
            p[MODE_DAY].textFaint = SColor(255, 130, 141, 154);
            p[MODE_DAY].accent = SColor(255, 10, 108, 214);
            p[MODE_DAY].accentText = SColor(255, 255, 255, 255);
            p[MODE_DAY].field = SColor(255, 248, 249, 251);
            p[MODE_DAY].danger = SColor(255, 199, 70, 60);
            p[MODE_DAY].dangerText = SColor(255, 255, 255, 255);
            p[MODE_DAY].ok = SColor(255, 22, 128, 60);
            p[MODE_DAY].error = SColor(255, 190, 40, 30);
            p[MODE_DAY].portText = SColor(255, 196, 30, 30);
            p[MODE_DAY].stbdText = SColor(255, 20, 140, 60);
            //Dusk: dark panels, white text
            p[MODE_DUSK].band = SColor(255, 13, 20, 30);
            p[MODE_DUSK].panelTop = SColor(255, 28, 42, 60);
            p[MODE_DUSK].panelBottom = SColor(255, 22, 33, 48);
            p[MODE_DUSK].edge = SColor(255, 44, 60, 82);
            p[MODE_DUSK].raised = SColor(255, 30, 45, 64);
            p[MODE_DUSK].key = SColor(255, 29, 43, 62);
            p[MODE_DUSK].keyHover = SColor(255, 38, 56, 79);
            p[MODE_DUSK].text = SColor(255, 215, 225, 236);
            p[MODE_DUSK].textDim = SColor(255, 142, 162, 184);
            p[MODE_DUSK].textFaint = SColor(255, 100, 117, 138);
            p[MODE_DUSK].accent = SColor(255, 74, 168, 255);
            p[MODE_DUSK].accentText = SColor(255, 255, 255, 255);
            p[MODE_DUSK].field = SColor(255, 14, 22, 34);
            p[MODE_DUSK].danger = SColor(255, 138, 46, 39);
            p[MODE_DUSK].dangerText = SColor(255, 255, 236, 232);
            p[MODE_DUSK].ok = SColor(255, 120, 220, 150);
            p[MODE_DUSK].error = SColor(255, 255, 120, 100);
            p[MODE_DUSK].portText = SColor(255, 240, 80, 70);
            p[MODE_DUSK].stbdText = SColor(255, 70, 210, 110);
            //Night: near black, dim amber - nothing bright enough to spoil night vision
            p[MODE_NIGHT].band = SColor(255, 6, 10, 16);
            p[MODE_NIGHT].panelTop = SColor(255, 15, 23, 34);
            p[MODE_NIGHT].panelBottom = SColor(255, 11, 17, 26);
            p[MODE_NIGHT].edge = SColor(255, 29, 40, 54);
            p[MODE_NIGHT].raised = SColor(255, 17, 26, 38);
            p[MODE_NIGHT].key = SColor(255, 14, 22, 33);
            p[MODE_NIGHT].keyHover = SColor(255, 20, 32, 48);
            p[MODE_NIGHT].text = SColor(255, 224, 170, 100);
            p[MODE_NIGHT].textDim = SColor(255, 150, 118, 78);
            p[MODE_NIGHT].textFaint = SColor(255, 90, 74, 54);
            p[MODE_NIGHT].accent = SColor(255, 240, 166, 60);
            p[MODE_NIGHT].accentText = SColor(255, 26, 16, 6);
            p[MODE_NIGHT].field = SColor(255, 7, 11, 17);
            p[MODE_NIGHT].danger = SColor(255, 110, 36, 30);
            p[MODE_NIGHT].dangerText = SColor(255, 230, 190, 170);
            p[MODE_NIGHT].ok = SColor(255, 150, 190, 110);
            p[MODE_NIGHT].error = SColor(255, 220, 110, 80);
            p[MODE_NIGHT].portText = SColor(255, 190, 70, 55);
            p[MODE_NIGHT].stbdText = SColor(255, 80, 160, 90);
            //Digital: the radar's colours - black, phosphor green, yellow captions
            p[MODE_DIGITAL].band = SColor(255, 0, 0, 0);
            p[MODE_DIGITAL].panelTop = SColor(255, 6, 13, 7);
            p[MODE_DIGITAL].panelBottom = SColor(255, 2, 6, 3);
            p[MODE_DIGITAL].edge = SColor(255, 22, 74, 30);
            p[MODE_DIGITAL].raised = SColor(255, 8, 20, 10);
            p[MODE_DIGITAL].key = SColor(255, 5, 14, 7);
            p[MODE_DIGITAL].keyHover = SColor(255, 12, 34, 16);
            p[MODE_DIGITAL].text = SColor(255, 92, 232, 104);
            p[MODE_DIGITAL].textDim = SColor(255, 214, 192, 56);
            p[MODE_DIGITAL].textFaint = SColor(255, 34, 100, 42);
            p[MODE_DIGITAL].accent = SColor(255, 60, 214, 82);
            p[MODE_DIGITAL].accentText = SColor(255, 0, 18, 4);
            p[MODE_DIGITAL].field = SColor(255, 0, 4, 1);
            p[MODE_DIGITAL].danger = SColor(255, 120, 30, 24);
            p[MODE_DIGITAL].dangerText = SColor(255, 255, 222, 206);
            p[MODE_DIGITAL].ok = SColor(255, 120, 245, 130);
            p[MODE_DIGITAL].error = SColor(255, 255, 110, 80);
            p[MODE_DIGITAL].portText = SColor(255, 240, 72, 60);
            p[MODE_DIGITAL].stbdText = SColor(255, 92, 236, 110);
        }
        if (mode < 0 || mode >= MODE_COUNT) { mode = currentMode(); }
        return p[mode];
    }

    //--- Small drawing helpers, all clipped -------------------------------------------------------

    //Rounded rectangle, built from horizontal strips so it clips like any 2D rectangle.
    //Radius up to about 6 px looks right at UI sizes.
    inline void fillRound(irr::video::IVideoDriver* d, const irr::core::rect<irr::s32>& r, irr::s32 rad,
        irr::video::SColor top, irr::video::SColor bottom, const irr::core::rect<irr::s32>* clip)
    {
        const irr::s32 w = r.getWidth(), h = r.getHeight();
        if (w <= 0 || h <= 0) { return; }
        if (rad * 2 > h) { rad = h / 2; }
        if (rad * 2 > w) { rad = w / 2; }
        for (irr::s32 i = 0; i < rad; i++) {
            const irr::f32 dy = (irr::f32)rad - (irr::f32)i - 0.5f;
            const irr::s32 inset = rad - (irr::s32)floorf(sqrtf((irr::f32)(rad * rad) - dy * dy) + 0.5f);
            d->draw2DRectangle(top, irr::core::rect<irr::s32>(r.UpperLeftCorner.X + inset, r.UpperLeftCorner.Y + i,
                r.LowerRightCorner.X - inset, r.UpperLeftCorner.Y + i + 1), clip);
            d->draw2DRectangle(bottom, irr::core::rect<irr::s32>(r.UpperLeftCorner.X + inset, r.LowerRightCorner.Y - i - 1,
                r.LowerRightCorner.X - inset, r.LowerRightCorner.Y - i), clip);
        }
        const irr::core::rect<irr::s32> mid(r.UpperLeftCorner.X, r.UpperLeftCorner.Y + rad, r.LowerRightCorner.X, r.LowerRightCorner.Y - rad);
        if (mid.getHeight() > 0) { d->draw2DRectangle(mid, top, top, bottom, bottom, clip); }
    }

    //Rounded rectangle with a one-pixel outline
    inline void frameRound(irr::video::IVideoDriver* d, const irr::core::rect<irr::s32>& r, irr::s32 rad,
        irr::video::SColor edge, irr::video::SColor top, irr::video::SColor bottom, const irr::core::rect<irr::s32>* clip)
    {
        fillRound(d, r, rad, edge, edge, clip);
        fillRound(d, irr::core::rect<irr::s32>(r.UpperLeftCorner.X + 1, r.UpperLeftCorner.Y + 1,
            r.LowerRightCorner.X - 1, r.LowerRightCorner.Y - 1), rad > 1 ? rad - 1 : rad, top, bottom, clip);
    }

    inline void disc(irr::video::IVideoDriver* d, irr::core::position2di c, irr::s32 radius, irr::video::SColor col,
        const irr::core::rect<irr::s32>* clip)
    {
        fillRound(d, irr::core::rect<irr::s32>(c.X - radius, c.Y - radius, c.X + radius, c.Y + radius), radius, col, col, clip);
    }

    //A line two pixels wide, for icons
    inline void thickLine(irr::video::IVideoDriver* d, irr::f32 x0, irr::f32 y0, irr::f32 x1, irr::f32 y1, irr::video::SColor col)
    {
        d->draw2DLine(irr::core::position2di((irr::s32)x0, (irr::s32)y0), irr::core::position2di((irr::s32)x1, (irr::s32)y1), col);
        const bool steep = fabsf(y1 - y0) > fabsf(x1 - x0);
        const irr::s32 ox = steep ? 1 : 0, oy = steep ? 0 : 1;
        d->draw2DLine(irr::core::position2di((irr::s32)x0 + ox, (irr::s32)y0 + oy), irr::core::position2di((irr::s32)x1 + ox, (irr::s32)y1 + oy), col);
    }

    inline void ring(irr::video::IVideoDriver* d, irr::f32 cx, irr::f32 cy, irr::f32 r, irr::video::SColor col,
        irr::f32 a0 = 0.0f, irr::f32 a1 = 360.0f)
    {
        const int n = 20;
        irr::f32 px = 0, py = 0;
        for (int i = 0; i <= n; i++) {
            const irr::f32 a = (a0 + (a1 - a0) * (irr::f32)i / (irr::f32)n) * 3.14159265f / 180.0f;
            const irr::f32 x = cx + r * sinf(a), y = cy - r * cosf(a);
            if (i > 0) { thickLine(d, px, py, x, y, col); }
            px = x; py = y;
        }
    }

    //Line icons for the command bar, drawn in a size x size box
    enum Icon {
        ICON_NONE = 0, ICON_INTERFACE, ICON_BINOCULARS, ICON_BEARING, ICON_VIEW, ICON_CONTROLS,
        ICON_LINES, ICON_LOG, ICON_DETACH, ICON_QUIT, ICON_HIDE, ICON_RADAR, ICON_SUN, ICON_MOON, ICON_WEATHER
    };

    inline void drawIcon(irr::video::IVideoDriver* d, Icon icon, irr::f32 x, irr::f32 y, irr::f32 s, irr::video::SColor c)
    {
        auto L = [&](irr::f32 ax, irr::f32 ay, irr::f32 bx, irr::f32 by) { thickLine(d, x + ax * s, y + ay * s, x + bx * s, y + by * s, c); };
        auto R = [&](irr::f32 cx, irr::f32 cy, irr::f32 r) { ring(d, x + cx * s, y + cy * s, r * s, c); };
        switch (icon) {
        case ICON_INTERFACE: //a screen with its lower strip
            L(0.08f, 0.2f, 0.92f, 0.2f); L(0.92f, 0.2f, 0.92f, 0.78f); L(0.92f, 0.78f, 0.08f, 0.78f); L(0.08f, 0.78f, 0.08f, 0.2f);
            L(0.08f, 0.6f, 0.92f, 0.6f);
            break;
        case ICON_BINOCULARS:
            R(0.27f, 0.66f, 0.2f); R(0.73f, 0.66f, 0.2f); L(0.38f, 0.38f, 0.62f, 0.38f);
            L(0.12f, 0.6f, 0.2f, 0.18f); L(0.88f, 0.6f, 0.8f, 0.18f);
            break;
        case ICON_BEARING: //compass ring and a bearing line
            R(0.5f, 0.5f, 0.38f); L(0.5f, 0.06f, 0.5f, 0.24f); L(0.5f, 0.5f, 0.78f, 0.22f);
            break;
        case ICON_VIEW: //an eye
            L(0.04f, 0.5f, 0.28f, 0.26f); L(0.28f, 0.26f, 0.72f, 0.26f); L(0.72f, 0.26f, 0.96f, 0.5f);
            L(0.04f, 0.5f, 0.28f, 0.74f); L(0.28f, 0.74f, 0.72f, 0.74f); L(0.72f, 0.74f, 0.96f, 0.5f);
            R(0.5f, 0.5f, 0.13f);
            break;
        case ICON_CONTROLS: //three sliders
            L(0.08f, 0.22f, 0.92f, 0.22f); L(0.08f, 0.5f, 0.92f, 0.5f); L(0.08f, 0.78f, 0.92f, 0.78f);
            R(0.66f, 0.22f, 0.09f); R(0.34f, 0.5f, 0.09f); R(0.76f, 0.78f, 0.09f);
            break;
        case ICON_LINES: //an anchor
            R(0.5f, 0.17f, 0.11f); L(0.5f, 0.28f, 0.5f, 0.9f); L(0.3f, 0.42f, 0.7f, 0.42f);
            L(0.14f, 0.62f, 0.3f, 0.86f); L(0.3f, 0.86f, 0.5f, 0.92f); L(0.5f, 0.92f, 0.7f, 0.86f); L(0.7f, 0.86f, 0.86f, 0.62f);
            break;
        case ICON_WEATHER: //a cloud
            ring(d, x + 0.31f * s, y + 0.6f * s, 0.17f * s, c, 200.0f, 360.0f);
            ring(d, x + 0.55f * s, y + 0.46f * s, 0.23f * s, c, 290.0f, 445.0f);
            ring(d, x + 0.76f * s, y + 0.62f * s, 0.15f * s, c, 20.0f, 180.0f);
            L(0.15f, 0.77f, 0.76f, 0.77f);
            break;
        case ICON_LOG: //a page of lines
            L(0.2f, 0.08f, 0.8f, 0.08f); L(0.8f, 0.08f, 0.8f, 0.92f); L(0.8f, 0.92f, 0.2f, 0.92f); L(0.2f, 0.92f, 0.2f, 0.08f);
            L(0.34f, 0.32f, 0.66f, 0.32f); L(0.34f, 0.5f, 0.66f, 0.5f); L(0.34f, 0.68f, 0.56f, 0.68f);
            break;
        case ICON_DETACH: //a window and an arrow leaving it
            L(0.08f, 0.36f, 0.08f, 0.92f); L(0.08f, 0.92f, 0.64f, 0.92f); L(0.64f, 0.92f, 0.64f, 0.62f); L(0.08f, 0.36f, 0.36f, 0.36f);
            L(0.46f, 0.08f, 0.92f, 0.08f); L(0.92f, 0.08f, 0.92f, 0.54f); L(0.92f, 0.08f, 0.4f, 0.6f);
            break;
        case ICON_QUIT: //power symbol
            ring(d, x + 0.5f * s, y + 0.55f * s, 0.36f * s, c, 35.0f, 325.0f); L(0.5f, 0.06f, 0.5f, 0.48f);
            break;
        case ICON_HIDE: //chevron down
            L(0.18f, 0.36f, 0.5f, 0.68f); L(0.5f, 0.68f, 0.82f, 0.36f);
            break;
        case ICON_RADAR:
            R(0.5f, 0.5f, 0.4f); R(0.5f, 0.5f, 0.2f); L(0.5f, 0.5f, 0.86f, 0.3f);
            break;
        case ICON_SUN:
            R(0.5f, 0.5f, 0.18f);
            for (int k = 0; k < 8; k++) {
                const irr::f32 a = (irr::f32)k * 0.785398f;
                L(0.5f + 0.3f * sinf(a), 0.5f - 0.3f * cosf(a), 0.5f + 0.44f * sinf(a), 0.5f - 0.44f * cosf(a));
            }
            break;
        case ICON_MOON:
            ring(d, x + 0.5f * s, y + 0.5f * s, 0.38f * s, c, 200.0f, 520.0f);
            ring(d, x + 0.66f * s, y + 0.38f * s, 0.3f * s, c, 215.0f, 415.0f);
            break;
        default:
            break;
        }
    }

    //---------------------------------------------------------------------------------------------
    //The skin. Every Irrlicht widget draws through these few functions, so restyling them restyles
    //the whole simulator; fonts, sizes, icons and texts are left to the wrapped default skin.
    class BridgeSkin : public irr::gui::IGUISkin
    {
    public:
        enum KeyStyle { KEY_NORMAL = 0, KEY_BAR, KEY_DANGER };

        BridgeSkin(irr::gui::IGUIEnvironment* environment, irr::gui::IGUISkin* wrapped)
            : env(environment), base(wrapped)
        {
            if (base) { base->grab(); }
            applyPalette();
        }

        virtual ~BridgeSkin()
        {
            if (base) { base->drop(); }
        }

        //Button IDs drawn as command-bar keys (flat until hovered, with an icon) or as a red key
        void setKeyStyle(irr::s32 id, KeyStyle style, Icon icon = ICON_NONE)
        {
            keyStyles[id] = style;
            keyIcons[id] = icon;
        }

        //Pushes the current palette into the colours the widgets read for their text
        void applyPalette()
        {
            if (!base) { return; }
            const Palette& p = palette();
            using namespace irr::gui;
            base->setColor(EGDC_3D_FACE, p.panelBottom);
            base->setColor(EGDC_3D_SHADOW, p.edge);
            base->setColor(EGDC_3D_DARK_SHADOW, p.edge);
            base->setColor(EGDC_3D_HIGH_LIGHT, p.edge);
            base->setColor(EGDC_3D_LIGHT, p.edge);
            base->setColor(EGDC_WINDOW, p.panelBottom);
            base->setColor(EGDC_SCROLLBAR, p.field);
            base->setColor(EGDC_EDITABLE, p.field);
            base->setColor(EGDC_FOCUSED_EDITABLE, p.field);
            base->setColor(EGDC_GRAY_EDITABLE, p.panelBottom);
            base->setColor(EGDC_ACTIVE_BORDER, p.raised);
            base->setColor(EGDC_INACTIVE_BORDER, p.raised);
            base->setColor(EGDC_ACTIVE_CAPTION, p.text);
            base->setColor(EGDC_INACTIVE_CAPTION, p.textDim);
            base->setColor(EGDC_BUTTON_TEXT, p.text);
            base->setColor(EGDC_WINDOW_SYMBOL, p.textDim);
            base->setColor(EGDC_GRAY_WINDOW_SYMBOL, p.textFaint);
            base->setColor(EGDC_GRAY_TEXT, p.textFaint);
            base->setColor(EGDC_HIGH_LIGHT, p.accent);
            base->setColor(EGDC_HIGH_LIGHT_TEXT, p.accentText);
            base->setColor(EGDC_TOOLTIP, p.text);
            base->setColor(EGDC_TOOLTIP_BACKGROUND, p.raised);
            base->setColor(EGDC_ICON, p.text);
            base->setColor(EGDC_ICON_HIGH_LIGHT, p.accentText);
        }

        //--- delegated to the wrapped skin ---
        virtual irr::video::SColor getColor(irr::gui::EGUI_DEFAULT_COLOR color) const { return base->getColor(color); }
        virtual void setColor(irr::gui::EGUI_DEFAULT_COLOR which, irr::video::SColor newColor) { base->setColor(which, newColor); }
        virtual irr::s32 getSize(irr::gui::EGUI_DEFAULT_SIZE size) const { return base->getSize(size); }
        virtual const wchar_t* getDefaultText(irr::gui::EGUI_DEFAULT_TEXT text) const { return base->getDefaultText(text); }
        virtual void setDefaultText(irr::gui::EGUI_DEFAULT_TEXT which, const wchar_t* newText) { base->setDefaultText(which, newText); }
        virtual void setSize(irr::gui::EGUI_DEFAULT_SIZE which, irr::s32 size) { base->setSize(which, size); }
        virtual irr::gui::IGUIFont* getFont(irr::gui::EGUI_DEFAULT_FONT which = irr::gui::EGDF_DEFAULT) const { return base->getFont(which); }
        virtual void setFont(irr::gui::IGUIFont* font, irr::gui::EGUI_DEFAULT_FONT which = irr::gui::EGDF_DEFAULT) { base->setFont(font, which); }
        virtual irr::gui::IGUISpriteBank* getSpriteBank() const { return base->getSpriteBank(); }
        virtual void setSpriteBank(irr::gui::IGUISpriteBank* bank) { base->setSpriteBank(bank); }
        virtual irr::u32 getIcon(irr::gui::EGUI_DEFAULT_ICON icon) const { return base->getIcon(icon); }
        virtual void setIcon(irr::gui::EGUI_DEFAULT_ICON icon, irr::u32 index) { base->setIcon(icon, index); }

        //--- buttons, and the thumb of a slider ---
        virtual void draw3DButtonPaneStandard(irr::gui::IGUIElement* element, const irr::core::rect<irr::s32>& rect,
            const irr::core::rect<irr::s32>* clip = 0)
        {
            drawKey(element, rect, clip, false);
        }

        virtual void draw3DButtonPanePressed(irr::gui::IGUIElement* element, const irr::core::rect<irr::s32>& rect,
            const irr::core::rect<irr::s32>* clip = 0)
        {
            drawKey(element, rect, clip, true);
        }

        //--- edit boxes, lists, combo boxes, check boxes, framed texts ---
        virtual void draw3DSunkenPane(irr::gui::IGUIElement* element, irr::video::SColor bgcolor, bool flat, bool fillBackGround,
            const irr::core::rect<irr::s32>& rect, const irr::core::rect<irr::s32>* clip = 0)
        {
            irr::video::IVideoDriver* d = env->getVideoDriver();
            const Palette& p = palette();
            const bool focused = element && env->hasFocus(element) &&
                (element->getType() == irr::gui::EGUIET_EDIT_BOX);
            const irr::video::SColor edge = focused ? p.accent : p.edge;
            if (element && element->getType() == irr::gui::EGUIET_CHECK_BOX) {
                //The tick box itself: a rounded square, filled with the accent once ticked (drawIcon)
                frameRound(d, rect, 3, p.edge, p.field, p.field, clip);
                return;
            }
            if (fillBackGround) {
                frameRound(d, rect, 4, edge, p.field, p.field, clip);
            }
            else {
                outlineRound(d, rect, 4, edge, clip);
            }
        }

        //--- windows ---
        virtual irr::core::rect<irr::s32> draw3DWindowBackground(irr::gui::IGUIElement* element, bool drawTitleBar,
            irr::video::SColor titleBarColor, const irr::core::rect<irr::s32>& rect, const irr::core::rect<irr::s32>* clip = 0,
            irr::core::rect<irr::s32>* checkClientArea = 0)
        {
            const Palette& p = palette();
            irr::gui::IGUIFont* font = getFont(irr::gui::EGDF_WINDOW);
            const irr::s32 fh = font ? (irr::s32)font->getDimension(L"Ag").Height : 14;
            const irr::s32 titleH = fh + 12;
            irr::core::rect<irr::s32> title(rect.UpperLeftCorner.X + 1, rect.UpperLeftCorner.Y + 1,
                rect.LowerRightCorner.X - 1, rect.UpperLeftCorner.Y + 1 + titleH);
            if (checkClientArea) {
                *checkClientArea = rect;
                checkClientArea->UpperLeftCorner.X += 1;
                checkClientArea->LowerRightCorner.X -= 1;
                checkClientArea->LowerRightCorner.Y -= 1;
                checkClientArea->UpperLeftCorner.Y += drawTitleBar ? titleH + 2 : 1;
                return title;
            }
            irr::video::IVideoDriver* d = env->getVideoDriver();
            //A soft shadow, the body, then the title strip with a hairline under it
            fillRound(d, irr::core::rect<irr::s32>(rect.UpperLeftCorner.X + 3, rect.UpperLeftCorner.Y + 5,
                rect.LowerRightCorner.X + 3, rect.LowerRightCorner.Y + 5), 7, irr::video::SColor(60, 0, 0, 0), irr::video::SColor(60, 0, 0, 0), clip);
            frameRound(d, rect, 7, p.edge, p.panelTop, p.panelBottom, clip);
            if (drawTitleBar) {
                fillRound(d, irr::core::rect<irr::s32>(title.UpperLeftCorner.X, title.UpperLeftCorner.Y,
                    title.LowerRightCorner.X, title.LowerRightCorner.Y + 6), 6, p.raised, p.raised, clip);
                d->draw2DRectangle(p.raised, irr::core::rect<irr::s32>(title.UpperLeftCorner.X, title.LowerRightCorner.Y - 6,
                    title.LowerRightCorner.X, title.LowerRightCorner.Y), clip);
                d->draw2DRectangle(p.edge, irr::core::rect<irr::s32>(title.UpperLeftCorner.X, title.LowerRightCorner.Y,
                    title.LowerRightCorner.X, title.LowerRightCorner.Y + 1), clip);
            }
            irr::core::rect<irr::s32> textRect = title;
            textRect.UpperLeftCorner.X += 6;
            return textRect;
        }

        virtual void draw3DMenuPane(irr::gui::IGUIElement* element, const irr::core::rect<irr::s32>& rect,
            const irr::core::rect<irr::s32>* clip = 0)
        {
            const Palette& p = palette();
            frameRound(env->getVideoDriver(), rect, 4, p.edge, p.panelTop, p.panelBottom, clip);
        }

        virtual void draw3DToolBar(irr::gui::IGUIElement* element, const irr::core::rect<irr::s32>& rect,
            const irr::core::rect<irr::s32>* clip = 0)
        {
            const Palette& p = palette();
            env->getVideoDriver()->draw2DRectangle(rect, p.raised, p.raised, p.panelBottom, p.panelBottom, clip);
        }

        //--- tabs: flat, the open one underlined in the accent colour ---
        virtual void draw3DTabButton(irr::gui::IGUIElement* element, bool active, const irr::core::rect<irr::s32>& rect,
            const irr::core::rect<irr::s32>* clip = 0, irr::gui::EGUI_ALIGNMENT alignment = irr::gui::EGUIA_UPPERLEFT)
        {
            irr::video::IVideoDriver* d = env->getVideoDriver();
            const Palette& p = palette();
            if (!active) { return; }
            d->draw2DRectangle(p.panelTop, irr::core::rect<irr::s32>(rect.UpperLeftCorner.X + 2, rect.UpperLeftCorner.Y + 2,
                rect.LowerRightCorner.X - 2, rect.LowerRightCorner.Y), clip);
            const irr::s32 y = (alignment == irr::gui::EGUIA_UPPERLEFT) ? rect.LowerRightCorner.Y - 3 : rect.UpperLeftCorner.Y;
            d->draw2DRectangle(p.accent, irr::core::rect<irr::s32>(rect.UpperLeftCorner.X + 4, y,
                rect.LowerRightCorner.X - 4, y + 3), clip);
        }

        virtual void draw3DTabBody(irr::gui::IGUIElement* element, bool border, bool background, const irr::core::rect<irr::s32>& rect,
            const irr::core::rect<irr::s32>* clip = 0, irr::s32 tabHeight = -1, irr::gui::EGUI_ALIGNMENT alignment = irr::gui::EGUIA_UPPERLEFT)
        {
            irr::video::IVideoDriver* d = env->getVideoDriver();
            const Palette& p = palette();
            if (tabHeight < 0) { tabHeight = getSize(irr::gui::EGDS_BUTTON_HEIGHT); }
            irr::core::rect<irr::s32> body = rect;
            if (alignment == irr::gui::EGUIA_UPPERLEFT) { body.UpperLeftCorner.Y += tabHeight + 2; }
            else { body.LowerRightCorner.Y -= tabHeight + 2; }
            if (background) {
                d->draw2DRectangle(body, p.panelTop, p.panelTop, p.panelBottom, p.panelBottom, clip);
            }
        }

        //--- the tick of a check box; everything else is the wrapped skin's sprite ---
        virtual void drawIcon(irr::gui::IGUIElement* element, irr::gui::EGUI_DEFAULT_ICON icon, const irr::core::position2di position,
            irr::u32 starttime = 0, irr::u32 currenttime = 0, bool loop = false, const irr::core::rect<irr::s32>* clip = 0)
        {
            if (icon == irr::gui::EGDI_CHECK_BOX_CHECKED) {
                irr::video::IVideoDriver* d = env->getVideoDriver();
                const Palette& p = palette();
                const irr::s32 h = getSize(irr::gui::EGDS_CHECK_BOX_WIDTH) / 2;
                const irr::core::rect<irr::s32> box(position.X - h, position.Y - h, position.X + h, position.Y + h);
                fillRound(d, box, 3, p.accent, p.accent, clip);
                const irr::f32 s = (irr::f32)(2 * h);
                const irr::f32 x = (irr::f32)box.UpperLeftCorner.X, y = (irr::f32)box.UpperLeftCorner.Y;
                thickLine(d, x + 0.24f * s, y + 0.52f * s, x + 0.42f * s, y + 0.7f * s, p.accentText);
                thickLine(d, x + 0.42f * s, y + 0.7f * s, x + 0.76f * s, y + 0.3f * s, p.accentText);
                return;
            }
            base->drawIcon(element, icon, position, starttime, currenttime, loop, clip);
        }

        //--- plain fills; a slider's track becomes a thin rail, filled up to the thumb ---
        virtual void draw2DRectangle(irr::gui::IGUIElement* element, const irr::video::SColor& color,
            const irr::core::rect<irr::s32>& pos, const irr::core::rect<irr::s32>* clip = 0)
        {
            irr::video::IVideoDriver* d = env->getVideoDriver();
            if (element && element->getType() == irr::gui::EGUIET_SCROLL_BAR && color == getColor(irr::gui::EGDC_SCROLLBAR)) {
                drawTrack((irr::gui::IGUIScrollBar*)element, pos, clip);
                return;
            }
            d->draw2DRectangle(color, pos, clip);
        }

        virtual irr::gui::EGUI_SKIN_TYPE getType() const { return irr::gui::EGST_UNKNOWN; }

    private:
        irr::gui::IGUIEnvironment* env;
        irr::gui::IGUISkin* base;
        std::map<irr::s32, int> keyStyles;
        std::map<irr::s32, Icon> keyIcons;

        static void outlineRound(irr::video::IVideoDriver* d, const irr::core::rect<irr::s32>& r, irr::s32 rad,
            irr::video::SColor edge, const irr::core::rect<irr::s32>* clip)
        {
            d->draw2DRectangle(edge, irr::core::rect<irr::s32>(r.UpperLeftCorner.X + rad, r.UpperLeftCorner.Y, r.LowerRightCorner.X - rad, r.UpperLeftCorner.Y + 1), clip);
            d->draw2DRectangle(edge, irr::core::rect<irr::s32>(r.UpperLeftCorner.X + rad, r.LowerRightCorner.Y - 1, r.LowerRightCorner.X - rad, r.LowerRightCorner.Y), clip);
            d->draw2DRectangle(edge, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y + rad, r.UpperLeftCorner.X + 1, r.LowerRightCorner.Y - rad), clip);
            d->draw2DRectangle(edge, irr::core::rect<irr::s32>(r.LowerRightCorner.X - 1, r.UpperLeftCorner.Y + rad, r.LowerRightCorner.X, r.LowerRightCorner.Y - rad), clip);
        }

        static bool parentIs(irr::gui::IGUIElement* e, irr::gui::EGUI_ELEMENT_TYPE type)
        {
            return e && e->getParent() && e->getParent()->getType() == type;
        }

        void drawTrack(irr::gui::IGUIScrollBar* bar, const irr::core::rect<irr::s32>& pos, const irr::core::rect<irr::s32>* clip)
        {
            irr::video::IVideoDriver* d = env->getVideoDriver();
            const Palette& p = palette();
            const bool horizontal = pos.getWidth() >= pos.getHeight();
            const irr::s32 range = bar->getMax() - bar->getMin();
            const irr::f32 t = (range > 0) ? (irr::f32)(bar->getPos() - bar->getMin()) / (irr::f32)range : 0.0f;
            if (horizontal) {
                const irr::s32 cy = pos.getCenter().Y;
                const irr::s32 x0 = pos.UpperLeftCorner.X + 2, x1 = pos.LowerRightCorner.X - 2;
                fillRound(d, irr::core::rect<irr::s32>(x0, cy - 2, x1, cy + 2), 2, p.edge, p.edge, clip);
                const irr::s32 xf = x0 + (irr::s32)((irr::f32)(x1 - x0) * t);
                if (xf > x0 + 1) { fillRound(d, irr::core::rect<irr::s32>(x0, cy - 2, xf, cy + 2), 2, p.accent, p.accent, clip); }
            }
            else {
                const irr::s32 cx = pos.getCenter().X;
                fillRound(d, irr::core::rect<irr::s32>(cx - 2, pos.UpperLeftCorner.Y + 2, cx + 2, pos.LowerRightCorner.Y - 2), 2, p.edge, p.edge, clip);
            }
        }

        void drawKey(irr::gui::IGUIElement* element, const irr::core::rect<irr::s32>& rect, const irr::core::rect<irr::s32>* clip, bool pressed)
        {
            irr::video::IVideoDriver* d = env->getVideoDriver();
            const Palette& p = palette();
            const bool hovered = element && env->getHovered() == element;

            //The thumb of a slider: a round knob on a horizontal rail, a pill on a vertical one
            if (element && element->getType() == irr::gui::EGUIET_SCROLL_BAR) {
                const bool horizontal = element->getAbsolutePosition().getWidth() > element->getAbsolutePosition().getHeight();
                if (horizontal) {
                    const irr::s32 r = irr::core::min_(rect.getHeight(), 18) / 2;
                    const irr::core::position2di c = rect.getCenter();
                    disc(d, c, r, p.accent, clip);
                    disc(d, c, r - 2, hovered ? p.keyHover : p.key, clip);
                }
                else {
                    const irr::s32 cx = rect.getCenter().X;
                    fillRound(d, irr::core::rect<irr::s32>(cx - 3, rect.UpperLeftCorner.Y + 1, cx + 3, rect.LowerRightCorner.Y - 1), 3,
                        hovered ? p.accent : p.textFaint, hovered ? p.accent : p.textFaint, clip);
                }
                return;
            }
            //Arrow buttons of sliders, lists and combo boxes, and a window's close box: flat
            if (parentIs(element, irr::gui::EGUIET_SCROLL_BAR) || parentIs(element, irr::gui::EGUIET_COMBO_BOX) ||
                parentIs(element, irr::gui::EGUIET_TAB_CONTROL)) {
                if (hovered || pressed) { fillRound(d, rect, 3, p.keyHover, p.keyHover, clip); }
                return;
            }

            const irr::s32 id = element ? element->getID() : -1;
            const std::map<irr::s32, int>::const_iterator style = keyStyles.find(id);
            const int kind = (style == keyStyles.end()) ? KEY_NORMAL : style->second;

            if (kind == KEY_DANGER) {
                fillRound(d, rect, 5, p.danger, p.danger, clip);
                if (hovered) { outlineRound(d, rect, 5, p.dangerText, clip); }
            }
            else if (kind == KEY_BAR) {
                //Command bar keys: flat on the bar, a key shape only when hovered or switched on
                if (pressed || hovered) {
                    frameRound(d, rect, 5, p.edge, p.keyHover, p.keyHover, clip);
                }
                if (pressed) {
                    d->draw2DRectangle(p.accent, irr::core::rect<irr::s32>(rect.UpperLeftCorner.X + 5, rect.LowerRightCorner.Y - 3,
                        rect.LowerRightCorner.X - 5, rect.LowerRightCorner.Y - 1), clip);
                }
            }
            else {
                const irr::video::SColor top = pressed ? p.panelBottom : (hovered ? p.keyHover : p.key);
                const irr::video::SColor bottom = pressed ? p.panelBottom : p.key;
                frameRound(d, rect, 5, (hovered && !pressed) ? p.accent : p.edge, top, bottom, clip);
            }

            //Icon to the left of the label. Labels with an icon start with spaces that leave it room.
            const std::map<irr::s32, Icon>::const_iterator ic = keyIcons.find(id);
            if (ic != keyIcons.end() && ic->second != ICON_NONE && element) {
                irr::gui::IGUIFont* font = getFont();
                const irr::s32 iconSize = iconSizeFor(rect);
                irr::s32 textW = 0;
                if (font && element->getText()) { textW = (irr::s32)font->getDimension(element->getText()).Width; }
                irr::s32 x = rect.getCenter().X - textW / 2;
                if (textW == 0) { x = rect.getCenter().X - iconSize / 2; }
                const irr::video::SColor col = (kind == KEY_DANGER) ? p.dangerText : (pressed ? p.accent : p.text);
                bridge::drawIcon(d, ic->second, (irr::f32)x, (irr::f32)(rect.getCenter().Y - iconSize / 2), (irr::f32)iconSize, col);
            }
        }

    public:
        static irr::s32 iconSizeFor(const irr::core::rect<irr::s32>& rect)
        {
            return irr::core::max_(10, irr::core::min_(18, rect.getHeight() - 14));
        }
    };

    //---------------------------------------------------------------------------------------------
    //The command bar: a panel under the console that the buttons sit on, with its separators and
    //the small captions ("ZOOM", "x1.0", "ECLAIRAGE"). It takes no input.
    class CommandBar : public irr::gui::IGUIElement
    {
    public:
        CommandBar(irr::gui::IGUIEnvironment* environment, irr::gui::IGUIElement* parent, const irr::core::rect<irr::s32>& r)
            : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, environment, parent, -1, r) {}

        void clearMarks() { separators.clear(); captions.clear(); }
        void addSeparator(irr::s32 x) { separators.push_back(x); }
        //A caption in absolute coordinates; dim = the small grey label style
        void addCaption(const irr::core::rect<irr::s32>& r, const irr::core::stringw& text, bool dim)
        {
            Caption c; c.r = r; c.text = text; c.dim = dim;
            captions.push_back(c);
        }
        void setCaptionText(size_t i, const irr::core::stringw& text) { if (i < captions.size()) { captions[i].text = text; } }
        void setCaptionsVisible(bool visible) { captionsShown = visible; }

        virtual bool isPointInside(const irr::core::position2d<irr::s32>& point) const { return false; }

        virtual void draw()
        {
            if (!IsVisible) { return; }
            irr::video::IVideoDriver* d = Environment->getVideoDriver();
            const Palette& p = palette();
            frameRound(d, AbsoluteRect, 8, p.edge, p.panelTop, p.panelBottom, &AbsoluteClippingRect);
            const irr::s32 y0 = AbsoluteRect.UpperLeftCorner.Y + AbsoluteRect.getHeight() / 4;
            const irr::s32 y1 = AbsoluteRect.LowerRightCorner.Y - AbsoluteRect.getHeight() / 4;
            for (size_t i = 0; i < separators.size(); i++) {
                d->draw2DRectangle(p.edge, irr::core::rect<irr::s32>(separators[i], y0, separators[i] + 1, y1), &AbsoluteClippingRect);
            }
            irr::gui::IGUISkin* skin = Environment->getSkin();
            irr::gui::IGUIFont* font = skin ? skin->getFont() : 0;
            if (font && captionsShown) {
                for (size_t i = 0; i < captions.size(); i++) {
                    font->draw(captions[i].text.c_str(), captions[i].r, captions[i].dim ? p.textDim : p.text, true, true, &AbsoluteClippingRect);
                }
            }
            IGUIElement::draw();
        }

    private:
        struct Caption { irr::core::rect<irr::s32> r; irr::core::stringw text; bool dim; };
        std::vector<irr::s32> separators;
        std::vector<Caption> captions;
        bool captionsShown = true;
    };
}

#endif
