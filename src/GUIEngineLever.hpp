/*   NAUTITECH - Simulateur de Navigation
     Engine control lever for the main bridge view (Bridge Command fork).

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __GUI_ENGINE_LEVER_HPP_INCLUDED__
#define __GUI_ENGINE_LEVER_HPP_INCLUDED__

//KYARA: bridge-style engine control lever, drawn like the instrument console (no textures).
//Drop-in replacement for the OutlineScrollBar used for the port/starboard engines: it derives from
//IGUIScrollBar, keeps the same -100..+100 range and sends the same EGET_SCROLL_BAR_CHANGED event,
//so MyEventReceiver needs no changes at all.
//
//Sign convention is Bridge Command's: pos -100 = full ahead (top of the lever), +100 = full astern
//(bottom), 0 = stop in the middle. Ahead is green, astern red, matching the existing panel colours.

#include "IGUIScrollBar.h"
#include "IGUIButton.h"
#include "GUIPanelDraw.hpp"

namespace irr
{
namespace gui
{

    class GUIEngineLever : public IGUIScrollBar
    {
    public:
        GUIEngineLever(IGUIEnvironment* environment, IGUIElement* parent, s32 id, core::rect<s32> rectangle,
            video::SColor aheadColour, video::SColor asternColour);
        virtual ~GUIEngineLever();

        //! Title shown on the lever plate (e.g. "Bbd"). Optional.
        void setLabel(const core::stringw& label) { Label = label; }

        virtual bool OnEvent(const SEvent& event);
        virtual void draw();
        virtual void OnPostRender(u32 timeMs);
        virtual void updateAbsolutePosition();

        virtual s32 getMax() const { return Max; }
        virtual void setMax(s32 max);
        virtual s32 getMin() const { return Min; }
        virtual void setMin(s32 min);
        virtual s32 getSmallStep() const { return SmallStep; }
        virtual void setSmallStep(s32 step) { SmallStep = (step > 0) ? step : 5; }
        virtual s32 getLargeStep() const { return LargeStep; }
        virtual void setLargeStep(s32 step) { LargeStep = (step > 0) ? step : 25; }
        virtual s32 getPos() const { return Pos; }
        virtual void setPos(s32 pos);

        //Not used by this design, but part of the IGUIScrollBar interface.
        virtual void setDrawBackground(bool draw) { DrawBackground = draw; }
        virtual bool isDrawBackgroundEnabled() const { return DrawBackground; }
        virtual IGUIButton* getUpLeftButton() const { return 0; }
        virtual IGUIButton* getDownRightButton() const { return 0; }

    private:
        f32 headerHeight() const;
        //Y coordinate of a lever position, and the position for a Y coordinate.
        f32 yFromPos(s32 pos) const;
        s32 posFromY(s32 y) const;
        void sendChanged();

        PanelBatch batch;
        core::stringw Label;
        video::SColor aheadCol, asternCol;

        s32 Pos, Min, Max, SmallStep, LargeStep;
        bool Dragging;
        bool DrawBackground;
    };

} // end namespace gui
} // end namespace irr

#endif
