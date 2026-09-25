// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __GUI_RECTANGLE_H_INCLUDED__
#define __GUI_RECTANGLE_H_INCLUDED__

#include "IGUIElement.h"

namespace irr
{
namespace gui
{

	class IGUIRectangle : public IGUIElement
	{
	public:

		//! constructor
		IGUIRectangle(IGUIEnvironment* environment, IGUIElement* parent, core::rect<s32> rectangle, bool showBorder = true);

		//! destructor
		virtual ~IGUIRectangle();

		//! draws the element and its children
		virtual void draw();

		void setFillColour(video::SColor c) { fillColour = c; doFill = true; }
		void setClickThrough(bool on) { clickThrough = on; }

		//! When used as an accent overlay, become invisible to hit-testing so the button beneath receives the click
		virtual bool isPointInside(const core::position2d<s32>& point) const
		{
			return clickThrough ? false : IGUIElement::isPointInside(point);
		}
	private:
		bool showBorder;
		bool doFill = false;
		bool clickThrough = false;
		video::SColor fillColour;

	};
}
}

#endif

