/*   NAUTITECH - Simulateur de Navigation
     Placement of the big instructor windows (controls, weather): over the middle of the view,
     docked to a side, or wherever they are dragged by their title bar.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __PANELDOCK_HPP_INCLUDED__
#define __PANELDOCK_HPP_INCLUDED__

#include "irrlicht.h"
#include <functional>

class HudFont;

namespace bridge
{
    enum PanelPlace { PLACE_CENTRE = 0, PLACE_RIGHT, PLACE_LEFT, PLACE_FREE };

    //Where the dock key sends the window next: centre -> right -> left -> centre (a dragged
    //window goes back to the centre)
    int nextPanelPlace(int place);

    //The window's side of it: the key in its title bar and the drag by the title bar. The window
    //(ControlsPanel, WeatherPanel) passes its mouse events and draws the key; what a place means on
    //the screen is decided by GUIMain, through the two callbacks.
    class PanelDock
    {
    public:
        std::function<void(int)> onPlace;                                 //the key was pressed: the place wanted
        std::function<void(const irr::core::rect<irr::s32>&)> onMoved;    //dropped there after a drag
        int place = PLACE_CENTRE;                                         //set by GUIMain

        //The key, just left of the close box
        irr::core::rect<irr::f32> keyRect(const irr::core::rect<irr::f32>& closeBox, HudFont* font, irr::f32 k) const;
        void draw(irr::video::IVideoDriver* driver, const irr::core::rect<irr::f32>& key, HudFont* font, irr::f32 k,
            const irr::core::rect<irr::s32>* clip) const;
        //A mouse event: true when it was used here (the key, or a drag of the title bar)
        bool onMouse(irr::gui::IGUIElement* window, irr::gui::IGUIEnvironment* env, const irr::SEvent::SMouseInput& m,
            const irr::core::rect<irr::f32>& titleBar, const irr::core::rect<irr::f32>& key, const irr::core::rect<irr::f32>& closeBox);
        bool isDragging() const { return dragging; }

    private:
        bool hover = false;
        bool dragging = false;
        irr::core::position2di grab;
        irr::core::rect<irr::s32> startRect;
    };
}

#endif
