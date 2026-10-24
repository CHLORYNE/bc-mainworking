// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __SCROLL_DIAL_H_INCLUDED__
#define __SCROLL_DIAL_H_INCLUDED__

#include "IGUIScrollBar.h"

namespace irr
{
namespace gui
{

	class ScrollDial : public IGUIScrollBar
	{
	public:

		//! constructor
		ScrollDial(core::position2d< s32 > centre, u32 radius, IGUIEnvironment* environment,
				IGUIElement* parent, s32 id, s32 maxAngle=315, bool showValue=false, bool noclip=false);

		//! destructor
		virtual ~ScrollDial();

		//CHANGES override the dial colour 
		void setOverrideColor(const video::SColor& c) { OverrideColor = c; HasOverride = true; }
		//! override the needle thickness (in pixels)
		void setLineThickness(u32 t) { LineThickness = t; }
		//radar GAIN CLUTTER RAIN CHANGE
		//! show a tick + number scale around the dial
		void setShowScale(bool on, s32 majorTicks = 4, s32 minorPerMajor = 5)
		{
			showScale = on; scaleMajor = majorTicks; scaleMinorPerMajor = minorPerMajor;
		}

		//! Radar console knob look: printed 300 degree scale (7 o'clock to 5 o'clock), lit value arc,
		//! fluted rubber skirt and metal cap with a pointer line, turning with the value. Operated
		//! like a real knob - grab and turn (no jump to where you click) - or with the mouse wheel.
		void setKnobStyle(bool on, video::SColor accent = video::SColor(255, 255, 176, 0))
		{
			knobStyle = on; knobAccent = accent;
		}

		//! called if an event happened.
		virtual bool OnEvent(const SEvent& event);

		//! draws the element and its children
		virtual void draw();

		virtual void OnPostRender(u32 timeMs);


		//! gets the maximum value of the scrollbar.
		virtual s32 getMax() const;

		//! sets the maximum value of the scrollbar.
		virtual void setMax(s32 max);

		//! gets the minimum value of the scrollbar.
		virtual s32 getMin() const;

		//! sets the minimum value of the scrollbar.
		virtual void setMin(s32 min);

		//! gets the small step value
		virtual s32 getSmallStep() const;

		//! sets the small step value
		virtual void setSmallStep(s32 step);

		//! gets the large step value
		virtual s32 getLargeStep() const;

		//! sets the large step value
		virtual void setLargeStep(s32 step);

		//! gets the current position of the scrollbar
		virtual s32 getPos() const;

		//! sets the position of the scrollbar
		virtual void setPos(s32 pos);

		//! updates the rectangle
		virtual void updateAbsolutePosition();
		//kyara update
		//! Sets whether to draw a background color (EGDC_SCROLLBAR)
		/** Ignored */
		virtual void setDrawBackground(bool draw);

		//! Checks if a background is drawn
		/** Ignored */
		virtual bool isDrawBackgroundEnabled() const;

		//! Access the up (vertical) or left (horizontal) button
		virtual IGUIButton* getUpLeftButton() const;

		//! Access the right (vertical) or down (horizontal) button
		virtual IGUIButton* getDownRightButton() const;


		//! Writes attributes of the element.
		//virtual void serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options) const;

		//! Reads attributes of the element
		//virtual void deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options);

	private:

		//void refreshControls();
		s32 getPosFromMousePos(const core::position2di &p) const;
		//CHANGES
		video::SColor OverrideColor = video::SColor(255, 0, 0, 0);
		bool       HasOverride = false;
		u32        LineThickness = 1;
		//Knob style (setKnobStyle)
		bool knobStyle = false;
		video::SColor knobAccent = video::SColor(255, 255, 176, 0);
		f32 knobLastAngle = 0;     //pointer angle at the previous drag event, deg
		f32 knobDragValue = 0;     //unrounded value while turning
		bool knobHovered = false;
		void drawKnob(const core::vector2d<s32>& absoluteCentre);
		bool knobEvent(const SEvent& event);
		f32 mouseAngle(const core::position2di& p) const;  //deg, clockwise from 12 o'clock
		void sendChanged();
		//BUTTON CHANGE RADAR
		bool showScale = false;
		s32  scaleMajor = 4;
		s32  scaleMinorPerMajor = 5;
		//IGUIButton* UpButton;
		//IGUIButton* DownButton;

		core::rect<s32> SliderRect;

		core::position2d< s32 > centre;
		u32 radius;

		bool Dragging;
		//bool Horizontal;
		//bool DraggedBySlider;
		//bool TrayClick;
		s32 Pos;
		s32 DrawPos;
		f32 DrawAngle;
		s32 DrawHeight;
		s32 Min;
		s32 Max;
		s32 SmallStep;
		s32 LargeStep;
		s32 DesiredPos;
		//u32 LastChange;
		video::SColor CurrentIconColor;

		s32 maxAngle;
		s32 thresholdAngle;

		bool showValue;

		f32 range () const { return (f32) ( Max - Min ); }
	};

} // end namespace gui
} // end namespace irr

#endif

