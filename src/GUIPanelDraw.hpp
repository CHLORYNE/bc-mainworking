/*   NAUTITECH - Simulateur de Navigation
     Shared 2D drawing helper for the instrument console and the engine levers.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __GUI_PANEL_DRAW_HPP_INCLUDED__
#define __GUI_PANEL_DRAW_HPP_INCLUDED__

     //KYARA: header-only geometry batch used by GUIInstrumentPanel and GUIEngineLever, so both draw
     //their bezels, gradients and needles the same way and there is only one copy to maintain.
     //Everything for one pass goes into a single vertex list and is sent as one draw call.

#include "IVideoDriver.h"
#include "S3DVertex.h"
//KYARA: include these explicitly. Some Irrlicht builds (including the svn tree this fork uses)
//only forward-declare SMaterial in IVideoDriver.h, which makes flush() below fail to compile with
//"uses undefined class irr::video::SMaterial". Naming what we use keeps this header portable.
#include "SMaterial.h"          //SMaterial, and EMT_TRANSPARENT_VERTEX_ALPHA via EMaterialTypes.h
#include "SColor.h"             //SColor
#include "EPrimitiveTypes.h"    //scene::EPT_TRIANGLES
#include "SVertexIndex.h"       //video::EIT_16BIT
#include "rect.h"
#include "vector2d.h"
#include <vector>
#include <cmath>

namespace irr
{
    namespace gui
    {

        inline video::SColor panelWithAlpha(video::SColor c, u32 a) { c.setAlpha(a); return c; }

        //Point at radius r and screen angle a (deg, clockwise from 12 o'clock)
        inline core::vector2df panelPolar(const core::vector2df& c, f32 r, f32 aDeg)
        {
            const f32 a = aDeg * core::DEGTORAD;
            return core::vector2df(c.X + r * sinf(a), c.Y - r * cosf(a));
        }

        inline f32 panelClamp(f32 v, f32 lo, f32 hi) { return v < lo ? lo : (v > hi ? hi : v); }

        class PanelBatch
        {
        public:
            void begin(video::IVideoDriver* d) { driver = d; verts.clear(); indices.clear(); }

            void flush()
            {
                if (!driver || verts.empty()) { verts.clear(); indices.clear(); return; }

                video::SMaterial m;
                m.Lighting = false;
                m.BackfaceCulling = false;
                m.MaterialType = video::EMT_TRANSPARENT_VERTEX_ALPHA; //vertex alpha drives the soft edges
                driver->setMaterial(m);

                //KYARA: 2D drawing ignores the material's culling flags and uses the driver's 2D
                //material instead. Switch culling off there for this one call, so no triangle can be
                //culled whatever the driver (OpenGL or D3D9) or Irrlicht version. Restored after.
                video::SMaterial& m2d = driver->getMaterial2D();
                const bool oldBack = m2d.BackfaceCulling, oldFront = m2d.FrontfaceCulling;
                m2d.BackfaceCulling = false;
                m2d.FrontfaceCulling = false;
                driver->enableMaterial2D(true);

                driver->draw2DVertexPrimitiveList(&verts[0], (u32)verts.size(), &indices[0], (u32)(indices.size() / 3),
                    video::EVT_STANDARD, scene::EPT_TRIANGLES, video::EIT_16BIT);

                driver->enableMaterial2D(false);
                m2d.BackfaceCulling = oldBack;
                m2d.FrontfaceCulling = oldFront;

                verts.clear();
                indices.clear();
            }

            void tri(const core::vector2df& a, video::SColor ca, const core::vector2df& b, video::SColor cb, const core::vector2df& c, video::SColor cc)
            {
                //KYARA: belt and braces with flush(): also keep every triangle clockwise on screen,
                //which is the winding Irrlicht's 2D mode keeps even when culling is on.
                const f32 cross = (b.X - a.X) * (c.Y - a.Y) - (b.Y - a.Y) * (c.X - a.X);
                if (fabsf(cross) < 1e-6f) { return; } //degenerate

                if (verts.size() > 64000) { flush(); } //16-bit index limit

                const u16 base = (u16)verts.size();
                verts.push_back(video::S3DVertex(a.X, a.Y, 0, 0, 0, 1, ca, 0, 0));
                if (cross > 0) {
                    verts.push_back(video::S3DVertex(b.X, b.Y, 0, 0, 0, 1, cb, 0, 0));
                    verts.push_back(video::S3DVertex(c.X, c.Y, 0, 0, 0, 1, cc, 0, 0));
                }
                else {
                    verts.push_back(video::S3DVertex(c.X, c.Y, 0, 0, 0, 1, cc, 0, 0));
                    verts.push_back(video::S3DVertex(b.X, b.Y, 0, 0, 0, 1, cb, 0, 0));
                }
                indices.push_back(base);
                indices.push_back(base + 1);
                indices.push_back(base + 2);
            }

            void tri(const core::vector2df& a, const core::vector2df& b, const core::vector2df& c, video::SColor col) { tri(a, col, b, col, c, col); }

            void quad(const core::vector2df& a, const core::vector2df& b, const core::vector2df& c, const core::vector2df& d, video::SColor col)
            {
                tri(a, b, c, col);
                tri(a, c, d, col);
            }

            void quad(const core::vector2df& a, video::SColor ca, const core::vector2df& b, video::SColor cb,
                const core::vector2df& c, video::SColor cc, const core::vector2df& d, video::SColor cd)
            {
                tri(a, ca, b, cb, c, cc);
                tri(a, ca, c, cc, d, cd);
            }

            //Annular sector between radii r0..r1 and screen angles a0..a1 (deg, clockwise from 12
            //o'clock). colIn at r0, colOut at r1. r0 = 0 gives a pie slice / disc. feather adds a 1px
            //alpha fade on the edges.
            void sector(const core::vector2df& c, f32 r0, f32 r1, f32 a0, f32 a1, video::SColor colIn, video::SColor colOut, bool feather = true)
            {
                if (r1 <= r0) { return; }
                if (a1 < a0) { const f32 t = a0; a0 = a1; a1 = t; }
                const f32 span = a1 - a0;
                if (span <= 0) { return; }

                //Segment count from the radius: smooth on big dials, cheap on small ones.
                s32 full = (s32)(r1 * 0.8f);
                if (full < 32) full = 32;
                if (full > 180) full = 180;
                s32 n = (s32)ceilf(full * span / 360.0f);
                if (n < 2) n = 2;

                const video::SColor inFade = panelWithAlpha(colIn, 0);
                const video::SColor outFade = panelWithAlpha(colOut, 0);
                const f32 fw = 1.1f; //feather width in px - a cheap anti-alias for round edges

                for (s32 i = 0; i < n; i++) {
                    const f32 t0 = a0 + span * (f32)i / (f32)n;
                    const f32 t1 = a0 + span * (f32)(i + 1) / (f32)n;
                    const core::vector2df o0 = panelPolar(c, r1, t0);
                    const core::vector2df o1 = panelPolar(c, r1, t1);

                    if (r0 <= 0.01f) {
                        tri(c, colIn, o0, colOut, o1, colOut);
                    }
                    else {
                        const core::vector2df i0 = panelPolar(c, r0, t0);
                        const core::vector2df i1 = panelPolar(c, r0, t1);
                        quad(i0, colIn, o0, colOut, o1, colOut, i1, colIn);
                        if (feather && r0 > fw) {
                            quad(panelPolar(c, r0 - fw, t0), inFade, i0, colIn, i1, colIn, panelPolar(c, r0 - fw, t1), inFade);
                        }
                    }
                    if (feather) {
                        quad(o0, colOut, panelPolar(c, r1 + fw, t0), outFade, panelPolar(c, r1 + fw, t1), outFade, o1, colOut);
                    }
                }
            }

            void disc(const core::vector2df& c, f32 r, video::SColor colCentre, video::SColor colEdge) { sector(c, 0, r, 0, 360, colCentre, colEdge, true); }

            //Full ring shaded light at the top and dark at the bottom - brushed-metal bezel.
            void bezelRing(const core::vector2df& c, f32 r0, f32 r1, video::SColor top, video::SColor bottom)
            {
                const s32 n = 96;
                for (s32 i = 0; i < n; i++) {
                    const f32 t0 = 360.0f * (f32)i / (f32)n;
                    const f32 t1 = 360.0f * (f32)(i + 1) / (f32)n;
                    //0 at the top, 1 at the bottom
                    const f32 k0 = 0.5f * (1.0f - cosf(t0 * core::DEGTORAD));
                    const f32 k1 = 0.5f * (1.0f - cosf(t1 * core::DEGTORAD));
                    const video::SColor c0 = bottom.getInterpolated(top, k0);
                    const video::SColor c1 = bottom.getInterpolated(top, k1);
                    //Inner edge uses the opposite shading - light from above means the inside lip is
                    //lit at the bottom.
                    const video::SColor i0 = top.getInterpolated(bottom, k0 * 0.6f + 0.2f);
                    const video::SColor i1 = top.getInterpolated(bottom, k1 * 0.6f + 0.2f);
                    quad(panelPolar(c, r0, t0), i0, panelPolar(c, r1, t0), c0, panelPolar(c, r1, t1), c1, panelPolar(c, r0, t1), i1);
                    //outer feather
                    quad(panelPolar(c, r1, t0), c0, panelPolar(c, r1 + 1.1f, t0), panelWithAlpha(c0, 0),
                        panelPolar(c, r1 + 1.1f, t1), panelWithAlpha(c1, 0), panelPolar(c, r1, t1), c1);
                }
            }

            void line(const core::vector2df& p0, const core::vector2df& p1, f32 width, video::SColor col)
            {
                core::vector2df d = p1 - p0;
                const f32 len = d.getLength();
                if (len < 1e-4f) { return; }
                d /= len;
                const core::vector2df n(-d.Y * width * 0.5f, d.X * width * 0.5f);
                quad(p0 + n, p1 + n, p1 - n, p0 - n, col);
            }

            void rect(const core::rect<f32>& r, video::SColor col)
            {
                quad(r.UpperLeftCorner, core::vector2df(r.LowerRightCorner.X, r.UpperLeftCorner.Y), r.LowerRightCorner,
                    core::vector2df(r.UpperLeftCorner.X, r.LowerRightCorner.Y), col);
            }

            //Vertical gradient rectangle
            void rectV(const core::rect<f32>& r, video::SColor top, video::SColor bottom)
            {
                quad(r.UpperLeftCorner, top, core::vector2df(r.LowerRightCorner.X, r.UpperLeftCorner.Y), top,
                    r.LowerRightCorner, bottom, core::vector2df(r.UpperLeftCorner.X, r.LowerRightCorner.Y), bottom);
            }

            //Horizontal gradient rectangle
            void rectH(const core::rect<f32>& r, video::SColor left, video::SColor right)
            {
                quad(r.UpperLeftCorner, left, core::vector2df(r.LowerRightCorner.X, r.UpperLeftCorner.Y), right,
                    r.LowerRightCorner, right, core::vector2df(r.UpperLeftCorner.X, r.LowerRightCorner.Y), left);
            }

        private:
            video::IVideoDriver* driver = 0;
            std::vector<video::S3DVertex> verts;
            std::vector<u16> indices;
        };

    } // end namespace gui
} // end namespace irr

#endif