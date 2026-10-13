/*   NAUTITECH - Simulateur de Navigation
     Helm control for the main bridge view, for a station without a hardware console.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __GUI_HELM_LEVER_HPP_INCLUDED__
#define __GUI_HELM_LEVER_HPP_INCLUDED__

//The engine levers' horizontal sister: same plate, same gate, same grip, laid across. It derives
//from IGUIScrollBar and is made with the wheel slider's id, so the order it sends is the very
//event the old wheel slider sent and MyEventReceiver needs no change.
//Sign convention is Bridge Command's: negative = to port (left), positive = to starboard (right),
//0 = midships. Port is red, starboard green.

#include "IGUIScrollBar.h"
#include "IGUIButton.h"
#include "GUIPanelDraw.hpp"

namespace irr
{
namespace gui
{

    class GUIHelmLever : public IGUIScrollBar
    {
    public:
        GUIHelmLever(IGUIEnvironment* environment, IGUIElement* parent, s32 id, core::rect<s32> rectangle);
        virtual ~GUIHelmLever();

        virtual bool OnEvent(const SEvent& event);
        virtual void draw();

        virtual s32 getMax() const { return Max; }
        virtual void setMax(s32 max);
        virtual s32 getMin() const { return Min; }
        virtual void setMin(s32 min);
        virtual s32 getSmallStep() const { return SmallStep; }
        virtual void setSmallStep(s32 step) { SmallStep = (step > 0) ? step : 1; }
        virtual s32 getLargeStep() const { return LargeStep; }
        virtual void setLargeStep(s32 step) { LargeStep = (step > 0) ? step : 5; }
        virtual s32 getPos() const { return Pos; }
        virtual void setPos(s32 pos);
        //Being dragged: the simulation's echo of the order is not written back meanwhile
        bool isDragging() const { return Dragging; }

        //Not used by this design, but part of the IGUIScrollBar interface.
        virtual void setDrawBackground(bool draw) { DrawBackground = draw; }
        virtual bool isDrawBackgroundEnabled() const { return DrawBackground; }
        virtual IGUIButton* getUpLeftButton() const { return 0; }
        virtual IGUIButton* getDownRightButton() const { return 0; }

    private:
        f32 headerHeight() const;
        //X coordinate of a helm position, and the position for an X coordinate.
        f32 xFromPos(s32 pos) const;
        s32 posFromX(s32 x) const;
        void sendChanged();

        PanelBatch batch;
        s32 Pos, Min, Max, SmallStep, LargeStep;
        bool Dragging;
        bool DrawBackground;
    };

} // end namespace gui
} // end namespace irr

#endif
