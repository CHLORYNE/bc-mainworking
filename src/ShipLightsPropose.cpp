/*   NAUTITECH - Simulateur de Navigation (Bridge Command fork)
     Proposed navigation lights for a vessel, from her 3D model. See ShipLights::proposeLights().

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "ShipLights.hpp"
#include "NavLight.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

//Two sources, in this order:
//  1. the lamps the modeller built: parts of the model named as lamps (light, lamp, lantern,
//     bulb, feu...) or small red / green parts high on the vessel. Read straight from the .obj
//     file, since the loaded mesh keeps no names. A column of them is the signal stack.
//  2. every navigation lamp still missing, placed on the hull's real shape (rays cast on the
//     model's triangles) by COLREG Annex I: masthead light on top of the mast, sidelights at the
//     sides of the wheelhouse no higher than 3/4 of the masthead light, sternlight on the
//     transom, anchor light at the bow, the signal stack on the mast (1 m apart under 20 m, 2 m
//     from 20 m), its lowest light at least 2 m (4 m from 20 m) above the deck.
//Everything is worked in the ship's frame, in metres: f towards the bow, t to starboard, u up
//from the waterline.

namespace
{
    struct ShipPoint { float v[3]; };   // [0] f, [1] t, [2] u
    struct Tri { ShipPoint p[3]; };

    //Rays along one axis of the ship's frame, through a grid of buckets on the other two
    class RayCaster
    {
    public:
        void build(const std::vector<Tri>& t)
        {
            tris = &t;
            for (int ax = 0; ax < 3; ax++) { buildGrid(ax); }
        }

        //Every crossing of the line along axis ax through (p, q) (the other two axes in order)
        void hits(int ax, float p, float q, std::vector<float>& out) const
        {
            out.clear();
            const Grid& g = grid[ax];
            if (g.cells.empty()) { return; }
            const int i = (int)std::floor((p - g.min1) / g.cell1);
            const int j = (int)std::floor((q - g.min2) / g.cell2);
            if (i < 0 || j < 0 || i >= N || j >= N) { return; }
            const std::vector<int>& list = g.cells[i * N + j];
            for (size_t k = 0; k < list.size(); k++) {
                const Tri& tr = (*tris)[list[k]];
                const float x0 = tr.p[0].v[g.a1], y0 = tr.p[0].v[g.a2];
                const float x1 = tr.p[1].v[g.a1], y1 = tr.p[1].v[g.a2];
                const float x2 = tr.p[2].v[g.a1], y2 = tr.p[2].v[g.a2];
                const float det = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
                if (std::fabs(det) < 1e-12f) { continue; }
                const float l0 = ((y1 - y2) * (p - x2) + (x2 - x1) * (q - y2)) / det;
                const float l1 = ((y2 - y0) * (p - x2) + (x0 - x2) * (q - y2)) / det;
                const float l2 = 1.0f - l0 - l1;
                if (l0 < 0.0f || l1 < 0.0f || l2 < 0.0f) { continue; }
                out.push_back(l0 * tr.p[0].v[ax] + l1 * tr.p[1].v[ax] + l2 * tr.p[2].v[ax]);
            }
        }

        bool maxHit(int ax, float p, float q, float& v) const
        {
            hits(ax, p, q, scratch);
            if (scratch.empty()) { return false; }
            v = *std::max_element(scratch.begin(), scratch.end());
            return true;
        }
        bool minHit(int ax, float p, float q, float& v) const
        {
            hits(ax, p, q, scratch);
            if (scratch.empty()) { return false; }
            v = *std::min_element(scratch.begin(), scratch.end());
            return true;
        }

    private:
        enum { N = 128 };
        struct Grid {
            int a1 = 0, a2 = 0;
            float min1 = 0, min2 = 0, cell1 = 1, cell2 = 1;
            std::vector<std::vector<int> > cells;
        };
        void buildGrid(int ax)
        {
            Grid& g = grid[ax];
            g.a1 = (ax == 0) ? 1 : 0;
            g.a2 = (ax == 2) ? 1 : 2;
            float lo1 = 1e30f, hi1 = -1e30f, lo2 = 1e30f, hi2 = -1e30f;
            for (size_t k = 0; k < tris->size(); k++) {
                for (int c = 0; c < 3; c++) {
                    const float a = (*tris)[k].p[c].v[g.a1], b = (*tris)[k].p[c].v[g.a2];
                    lo1 = std::min(lo1, a); hi1 = std::max(hi1, a);
                    lo2 = std::min(lo2, b); hi2 = std::max(hi2, b);
                }
            }
            if (lo1 > hi1) { return; }
            g.min1 = lo1; g.min2 = lo2;
            g.cell1 = std::max((hi1 - lo1) / N, 1e-4f) * 1.0001f;
            g.cell2 = std::max((hi2 - lo2) / N, 1e-4f) * 1.0001f;
            g.cells.assign(N * N, std::vector<int>());
            for (size_t k = 0; k < tris->size(); k++) {
                const Tri& tr = (*tris)[k];
                float a0 = tr.p[0].v[g.a1], a1 = a0, b0 = tr.p[0].v[g.a2], b1 = b0;
                for (int c = 1; c < 3; c++) {
                    a0 = std::min(a0, tr.p[c].v[g.a1]); a1 = std::max(a1, tr.p[c].v[g.a1]);
                    b0 = std::min(b0, tr.p[c].v[g.a2]); b1 = std::max(b1, tr.p[c].v[g.a2]);
                }
                const int i0 = std::max(0, (int)((a0 - g.min1) / g.cell1)), i1 = std::min(N - 1, (int)((a1 - g.min1) / g.cell1));
                const int j0 = std::max(0, (int)((b0 - g.min2) / g.cell2)), j1 = std::min(N - 1, (int)((b1 - g.min2) / g.cell2));
                for (int i = i0; i <= i1; i++) {
                    for (int j = j0; j <= j1; j++) { g.cells[i * N + j].push_back((int)k); }
                }
            }
        }
        const std::vector<Tri>* tris = 0;
        Grid grid[3];
        mutable std::vector<float> scratch;
    };

    //A lamp found in the model file
    enum LampColour { LC_WHITE = 0, LC_RED, LC_GREEN, LC_YELLOW };
    struct FoundLamp {
        float f, t, u;
        LampColour colour;
        bool strong = false;   // named as a lamp (the others only look like a lens)
        bool used = false;
    };

    std::string lower(std::string s)
    {
        for (size_t i = 0; i < s.size(); i++) { s[i] = (char)std::tolower((unsigned char)s[i]); }
        return s;
    }

    //The words of a material name: "Light.002" -> light ; "nav_light_port" -> nav, light, port
    std::vector<std::string> words(const std::string& name)
    {
        std::vector<std::string> out;
        std::string w;
        for (size_t i = 0; i <= name.size(); i++) {
            const char c = (i < name.size()) ? (char)std::tolower((unsigned char)name[i]) : ' ';
            if (c >= 'a' && c <= 'z') { w.push_back(c); }
            else if (!w.empty()) { out.push_back(w); w.clear(); }
        }
        return out;
    }

    //Named as a lamp outright
    bool lampName(const std::string& name)
    {
        static const char* exact[] = { "light", "lights", "lamp", "lamps", "lantern", "lanterns", "bulb",
            "lightbulb", "navlight", "navlights", "feu", "feux", "fanal", "masthead", "sidelight",
            "sternlight", "anchorlight", "toplight", "led", "lumiere", "lampe", "lanterne", 0 };
        const std::vector<std::string> w = words(name);
        for (size_t i = 0; i < w.size(); i++) {
            for (int k = 0; exact[k]; k++) { if (w[i] == exact[k]) { return true; } }
            if (w[i].find("lightbulb") != std::string::npos || w[i].find("navlight") != std::string::npos ||
                w[i].find("lantern") != std::string::npos) { return true; }
        }
        return false;
    }

    //Named as a side, or only by a lamp colour: a lamp only if it is small and high
    bool weakName(const std::string& name, LampColour& colour)
    {
        const std::vector<std::string> w = words(name);
        for (size_t i = 0; i < w.size(); i++) {
            if (w[i] == "port" || w[i] == "babord" || w[i] == "red" || w[i] == "rouge") { colour = LC_RED; return true; }
            if (w[i] == "stbd" || w[i] == "starboard" || w[i] == "tribord" || w[i] == "green" || w[i] == "vert") { colour = LC_GREEN; return true; }
        }
        return false;
    }

    LampColour colourOf(float r, float g, float b)
    {
        if (r > 0.55f && g < 0.35f && b < 0.35f) { return LC_RED; }
        if (g > 0.45f && r < 0.45f) { return LC_GREEN; }
        if (r > 0.75f && g > 0.6f && b < 0.45f) { return LC_YELLOW; }
        return LC_WHITE;
    }

    std::string dirOf(const std::string& path)
    {
        const size_t s = path.find_last_of("/\\");
        return (s == std::string::npos) ? std::string() : path.substr(0, s + 1);
    }

    //Diffuse colours of the materials of an .mtl file
    void readMtl(const std::string& path, std::map<std::string, irr::core::vector3df>& kd)
    {
        std::ifstream in(path.c_str());
        if (!in.is_open()) { return; }
        std::string line, current;
        while (std::getline(in, line)) {
            if (!line.empty() && line[line.size() - 1] == '\r') { line.erase(line.size() - 1); }
            std::istringstream ls(line);
            std::string key;
            ls >> key;
            if (key == "newmtl") { std::getline(ls >> std::ws, current); }
            else if (key == "Kd" && !current.empty()) {
                float r = 1, g = 1, b = 1;
                ls >> r >> g >> b;
                kd[current] = irr::core::vector3df(r, g, b);
            }
        }
    }
}

bool ShipLights::proposeLights(std::wstring& report)
{
    if (!editing || !smgrStored || !shipNodeStored) { return false; }
    if (shipNodeStored->getType() != irr::scene::ESNT_ANIMATED_MESH) {
        report = L"Mod\u00E8le 3D du navire illisible : rien n'a \u00E9t\u00E9 propos\u00E9.";
        return false;
    }
    irr::scene::IAnimatedMeshSceneNode* node = static_cast<irr::scene::IAnimatedMeshSceneNode*>(shipNodeStored);
    irr::scene::IAnimatedMesh* amesh = node->getMesh();
    irr::scene::IMesh* mesh = amesh ? amesh->getMesh(0) : 0;
    if (!mesh) {
        report = L"Mod\u00E8le 3D du navire illisible : rien n'a \u00E9t\u00E9 propos\u00E9.";
        return false;
    }

    //---- the ship's frame ------------------------------------------------------------------
    const irr::core::vector3df bow = bowDir(), stbd = stbdDir(), centre = boxCentre();
    const float m = (mupm > 0.0f) ? mupm : 1.0f;
    auto toShip = [&](const irr::core::vector3df& p) {
        ShipPoint s;
        const irr::core::vector3df d = p - centre;
        s.v[0] = d.dotProduct(bow) / m;
        s.v[1] = d.dotProduct(stbd) / m;
        s.v[2] = (p.Y - waterlineY) / m;
        return s;
    };
    auto toModel = [&](float f, float t, float u) {
        irr::core::vector3df p = centre + bow * (f * m) + stbd * (t * m);
        p.Y = waterlineY + u * m;
        return p;
    };

    std::vector<Tri> tris;
    float fMin = 1e30f, fMax = -1e30f, tMin = 1e30f, tMax = -1e30f, uMax = -1e30f;
    for (irr::u32 b = 0; b < mesh->getMeshBufferCount(); b++) {
        const irr::scene::IMeshBuffer* mb = mesh->getMeshBuffer(b);
        const irr::u32 ic = mb->getIndexCount();
        const bool idx32 = (mb->getIndexType() == irr::video::EIT_32BIT);
        const irr::u16* i16 = mb->getIndices();
        const irr::u32* i32 = reinterpret_cast<const irr::u32*>(i16);
        for (irr::u32 i = 0; i + 2 < ic; i += 3) {
            Tri tr;
            bool degenerate = true;
            for (int c = 0; c < 3; c++) {
                const irr::u32 vi = idx32 ? i32[i + c] : (irr::u32)i16[i + c];
                if (vi >= mb->getVertexCount()) { degenerate = true; break; }
                tr.p[c] = toShip(mb->getPosition(vi));
                if (c > 0 && (tr.p[c].v[0] != tr.p[0].v[0] || tr.p[c].v[1] != tr.p[0].v[1] ||
                              tr.p[c].v[2] != tr.p[0].v[2])) { degenerate = false; }
            }
            if (degenerate) { continue; } //parts hidden by MakeTransparent are scaled to a point
            for (int c = 0; c < 3; c++) {
                fMin = std::min(fMin, tr.p[c].v[0]); fMax = std::max(fMax, tr.p[c].v[0]);
                tMin = std::min(tMin, tr.p[c].v[1]); tMax = std::max(tMax, tr.p[c].v[1]);
                uMax = std::max(uMax, tr.p[c].v[2]);
            }
            tris.push_back(tr);
        }
    }
    if (tris.empty() || fMax - fMin < 0.5f) {
        report = L"Mod\u00E8le 3D du navire vide : rien n'a \u00E9t\u00E9 propos\u00E9.";
        return false;
    }
    RayCaster rc;
    rc.build(tris);
    const float span = fMax - fMin, fMid = 0.5f * (fMin + fMax);
    const float halfBeam = std::max(0.5f * (tMax - tMin), 0.3f);
    const float L = lengthMetres;
    const float spacingRule = (L < 20.0f) ? 1.0f : 2.0f;
    const float stackLift = (L < 20.0f) ? 2.0f : 4.0f; //lowest light of the line above the deck
    auto top = [&](float f, float t, float& u) { return rc.maxHit(2, f, t, u); };

    //---- 1. the lamps in the model file (.obj) ----------------------------------------------
    std::vector<FoundLamp> found;
    std::wstring modelNote;
    {
        const std::string path = smgrStored->getMeshCache()->getMeshName(amesh).getPath().c_str();
        if (lower(path).size() < 4 || lower(path).substr(lower(path).size() - 4) != ".obj") {
            modelNote = L"mod\u00E8le non .obj : lampes du mod\u00E8le non lues";
        } else {
            std::ifstream in(path.c_str());
            std::vector<irr::core::vector3df> verts;
            std::vector<std::string> mtlNames;
            std::map<std::string, int> mtlIndex;
            std::map<std::string, irr::core::vector3df> kd;
            std::vector<std::vector<irr::u32> > facesOf; //per material: vertex index triples
            int cur = -1;
            std::string line;
            while (in.is_open() && std::getline(in, line)) {
                if (line.size() < 2) { continue; }
                if (line[0] == 'v' && line[1] == ' ') {
                    float x = 0, y = 0, z = 0;
                    std::sscanf(line.c_str() + 2, "%f %f %f", &x, &y, &z);
                    verts.push_back(irr::core::vector3df(x, y, z));
                } else if (line.compare(0, 7, "usemtl ") == 0) {
                    std::string n = line.substr(7);
                    while (!n.empty() && (n[n.size() - 1] == '\r' || n[n.size() - 1] == ' ')) { n.erase(n.size() - 1); }
                    std::map<std::string, int>::iterator it = mtlIndex.find(n);
                    if (it == mtlIndex.end()) {
                        cur = (int)mtlNames.size();
                        mtlIndex[n] = cur;
                        mtlNames.push_back(n);
                        facesOf.push_back(std::vector<irr::u32>());
                    } else { cur = it->second; }
                } else if (line.compare(0, 7, "mtllib ") == 0) {
                    std::string n = line.substr(7);
                    while (!n.empty() && (n[n.size() - 1] == '\r' || n[n.size() - 1] == ' ')) { n.erase(n.size() - 1); }
                    readMtl(dirOf(path) + n, kd);
                } else if (line[0] == 'f' && line[1] == ' ' && cur >= 0) {
                    std::istringstream ls(line.substr(2));
                    std::string tok;
                    std::vector<long> idx;
                    while (ls >> tok) {
                        long v = std::atol(tok.c_str());
                        if (v < 0) { v = (long)verts.size() + v + 1; }
                        idx.push_back(v - 1);
                    }
                    for (size_t k = 1; k + 1 < idx.size(); k++) { //fan
                        if (idx[0] < 0 || idx[k] < 0 || idx[k + 1] < 0) { continue; }
                        facesOf[cur].push_back((irr::u32)idx[0]);
                        facesOf[cur].push_back((irr::u32)idx[k]);
                        facesOf[cur].push_back((irr::u32)idx[k + 1]);
                    }
                }
            }
            //The file's axes against the loaded mesh: Irrlicht turns an .obj round by X
            irr::core::aabbox3df fileBox;
            bool first = true;
            for (size_t i = 0; i < verts.size(); i++) {
                if (first) { fileBox.reset(verts[i]); first = false; } else { fileBox.addInternalPoint(verts[i]); }
            }
            const irr::core::aabbox3df meshBox = mesh->getBoundingBox();
            const float tol = 0.02f * std::max(1e-6f, meshBox.getExtent().getLength());
            int flipX = 0;
            if (!first) {
                if (std::fabs(meshBox.MinEdge.X + fileBox.MaxEdge.X) < tol && std::fabs(meshBox.MaxEdge.X + fileBox.MinEdge.X) < tol &&
                    std::fabs(meshBox.MinEdge.Y - fileBox.MinEdge.Y) < tol && std::fabs(meshBox.MaxEdge.Z - fileBox.MaxEdge.Z) < tol) {
                    flipX = 1;
                } else if (std::fabs(meshBox.MinEdge.X - fileBox.MinEdge.X) < tol && std::fabs(meshBox.MaxEdge.X - fileBox.MaxEdge.X) < tol &&
                    std::fabs(meshBox.MinEdge.Y - fileBox.MinEdge.Y) < tol && std::fabs(meshBox.MaxEdge.Z - fileBox.MaxEdge.Z) < tol) {
                    flipX = -1;
                }
            }
            if (flipX == 0) {
                modelNote = L"fichier .obj diff\u00E9rent du mod\u00E8le charg\u00E9 : lampes du mod\u00E8le non lues";
            } else {
                auto fileToShip = [&](const irr::core::vector3df& v) {
                    return toShip(irr::core::vector3df(flipX > 0 ? -v.X : v.X, v.Y, v.Z));
                };
                struct Cluster { float lo[3], hi[3]; int faces; };
                std::vector<FoundLamp> weak;
                for (size_t mi = 0; mi < mtlNames.size(); mi++) {
                    const std::vector<irr::u32>& fv = facesOf[mi];
                    const size_t nf = fv.size() / 3;
                    if (nf == 0 || nf > 3000) { continue; } //a lamp is a small part
                    LampColour nameColour = LC_WHITE;
                    const bool strong = lampName(mtlNames[mi]);
                    const bool weakN = !strong && weakName(mtlNames[mi], nameColour);
                    LampColour col = LC_WHITE;
                    bool haveKd = false;
                    {
                        std::map<std::string, irr::core::vector3df>::iterator it = kd.find(mtlNames[mi]);
                        if (it != kd.end()) { col = colourOf(it->second.X, it->second.Y, it->second.Z); haveKd = true; }
                    }
                    if (!haveKd && weakN) { col = nameColour; }
                    //Only by colour: small red or green parts (sidelight lenses)
                    const bool byColour = !strong && !weakN && (col == LC_RED || col == LC_GREEN);
                    if (!strong && !weakN && !byColour) { continue; }
                    if (strong && col == LC_YELLOW) {
                        //a yellow bulb glass is a white lamp; only a working light is yellow
                        const std::string ln = lower(mtlNames[mi]);
                        if (ln.find("deck") == std::string::npos && ln.find("flood") == std::string::npos &&
                            ln.find("work") == std::string::npos && ln.find("pont") == std::string::npos) { col = LC_WHITE; }
                    }

                    //Faces 30 cm apart or less belong to one fitting
                    std::vector<ShipPoint> centres(nf);
                    std::vector<Cluster> boxes(nf);
                    for (size_t k = 0; k < nf; k++) {
                        Cluster c;
                        for (int a = 0; a < 3; a++) { c.lo[a] = 1e30f; c.hi[a] = -1e30f; }
                        for (int j = 0; j < 3; j++) {
                            const irr::u32 vi = fv[3 * k + j];
                            if (vi >= verts.size()) { continue; }
                            const ShipPoint sp = fileToShip(verts[vi]);
                            for (int a = 0; a < 3; a++) { c.lo[a] = std::min(c.lo[a], sp.v[a]); c.hi[a] = std::max(c.hi[a], sp.v[a]); }
                        }
                        c.faces = 1;
                        boxes[k] = c;
                        for (int a = 0; a < 3; a++) { centres[k].v[a] = 0.5f * (c.lo[a] + c.hi[a]); }
                    }
                    std::vector<int> parent(nf);
                    for (size_t k = 0; k < nf; k++) { parent[k] = (int)k; }
                    std::function<int(int)> findRoot = [&](int x) { while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; };
                    for (size_t a = 0; a < nf; a++) {
                        for (size_t b = a + 1; b < nf; b++) {
                            const float df = centres[a].v[0] - centres[b].v[0], dt = centres[a].v[1] - centres[b].v[1], du = centres[a].v[2] - centres[b].v[2];
                            if (df * df + dt * dt + du * du < 0.09f) {
                                const int ra = findRoot((int)a), rb = findRoot((int)b);
                                if (ra != rb) { parent[ra] = rb; }
                            }
                        }
                    }
                    std::map<int, Cluster> groups;
                    for (size_t k = 0; k < nf; k++) {
                        const int r = findRoot((int)k);
                        std::map<int, Cluster>::iterator it = groups.find(r);
                        if (it == groups.end()) { groups[r] = boxes[k]; }
                        else {
                            for (int a = 0; a < 3; a++) {
                                it->second.lo[a] = std::min(it->second.lo[a], boxes[k].lo[a]);
                                it->second.hi[a] = std::max(it->second.hi[a], boxes[k].hi[a]);
                            }
                            it->second.faces++;
                        }
                    }
                    for (std::map<int, Cluster>::iterator it = groups.begin(); it != groups.end(); ++it) {
                        const Cluster& c = it->second;
                        float extent = 0.0f;
                        for (int a = 0; a < 3; a++) { extent = std::max(extent, c.hi[a] - c.lo[a]); }
                        FoundLamp fl;
                        fl.f = 0.5f * (c.lo[0] + c.hi[0]);
                        fl.t = 0.5f * (c.lo[1] + c.hi[1]);
                        fl.u = 0.5f * (c.lo[2] + c.hi[2]);
                        fl.colour = col;
                        fl.strong = strong;
                        if (strong && extent < 1.2f && fl.u > 0.5f) { found.push_back(fl); }
                        else if (!strong && extent < 0.3f && fl.u > 1.0f && (col == LC_RED || col == LC_GREEN)) { weak.push_back(fl); }
                    }
                }
                //Parts only coloured or named like a side are ambiguous: a pair is a pair of lenses,
                //more is paint
                if (weak.size() <= 2) { found.insert(found.end(), weak.begin(), weak.end()); }

                //One fitting made of several parts of a colour (base, glass, filament): one lamp,
                //the highest part (the glass)
                for (size_t a = 0; a < found.size(); a++) {
                    for (size_t b = a + 1; b < found.size(); ) {
                        const float df = found[a].f - found[b].f, dt = found[a].t - found[b].t, du = found[a].u - found[b].u;
                        if (df * df + dt * dt + du * du < 0.25f * 0.25f && found[a].colour == found[b].colour) {
                            if (found[b].u > found[a].u) { found[a].u = found[b].u; }
                            found.erase(found.begin() + b);
                        } else { b++; }
                    }
                }
            }
        }
    }

    //---- what the model gives ----------------------------------------------------------------
    struct Placed { Role role; irr::core::vector3df pos; irr::video::SColor colour; int origin; };
    std::vector<Placed> placed;
    bool haveStack = false;
    irr::core::vector3df stackBase;
    float stackSpacing = spacingRule;
    int stackOrigin = 0;
    auto have = [&](Role r) { for (size_t i = 0; i < placed.size(); i++) { if (placed[i].role == r) { return true; } } return false; };
    const irr::video::SColor WHITE(255, 255, 250, 235), RED(255, 255, 40, 25), GREEN(255, 30, 255, 60), YELLOW(255, 255, 226, 170);

    //A column of lamps (two or more, one above the other) is the signal line
    {
        std::vector<int> best;
        for (size_t a = 0; a < found.size(); a++) {
            std::vector<int> col;
            for (size_t b = 0; b < found.size(); b++) {
                const float df = found[a].f - found[b].f, dt = found[a].t - found[b].t;
                if (df * df + dt * dt < 0.3f * 0.3f) { col.push_back((int)b); }
            }
            if (col.size() < 2) { continue; }
            float lo = 1e30f, hi = -1e30f;
            for (size_t k = 0; k < col.size(); k++) { lo = std::min(lo, found[col[k]].u); hi = std::max(hi, found[col[k]].u); }
            if (hi - lo < 0.15f) { continue; } //side by side, not one above the other
            if (col.size() > best.size()) { best = col; }
        }
        if (!best.empty()) {
            float lo = 1e30f, hi = -1e30f, f = 0, t = 0;
            for (size_t k = 0; k < best.size(); k++) {
                const FoundLamp& fl = found[best[k]];
                lo = std::min(lo, fl.u); hi = std::max(hi, fl.u);
                f += fl.f; t += fl.t;
                found[best[k]].used = true;
            }
            f /= best.size(); t /= best.size();
            haveStack = true;
            stackSpacing = std::max(0.25f, (hi - lo) / (float)(best.size() - 1));
            stackBase = toModel(f, t, lo);
            stackOrigin = 1;
        }
    }
    //Red and green: the sidelights, the one furthest to its side. A red to starboard or a green
    //to port is not a sidelight; one found alone gives the other by symmetry.
    const float tCentre = 0.5f * (tMin + tMax);
    int sidePick[2] = { -1, -1 };
    for (int side = 0; side < 2; side++) {
        const LampColour want = side ? LC_GREEN : LC_RED;
        int pick = -1;
        for (size_t i = 0; i < found.size(); i++) {
            if (found[i].used || found[i].colour != want) { continue; }
            const float off = found[i].t - tCentre;
            //(a red and a green side by side are one bicolour lantern: both halves are right)
            bool pair = false;
            for (size_t k = 0; k < found.size(); k++) {
                if (found[k].colour != (side ? LC_RED : LC_GREEN)) { continue; }
                const float df = found[k].f - found[i].f, dt = found[k].t - found[i].t, du = found[k].u - found[i].u;
                if (df * df + dt * dt + du * du < 0.5f * 0.5f) { pair = true; }
            }
            if (!pair && (side ? off < -0.3f : off > 0.3f)) { continue; }
            if (pick < 0 || (side ? found[i].t > found[pick].t : found[i].t < found[pick].t)) { pick = (int)i; }
        }
        sidePick[side] = pick;
        if (pick >= 0) {
            found[pick].used = true;
            placed.push_back({ side ? ROLE_STARBOARD : ROLE_PORT, toModel(found[pick].f, found[pick].t, found[pick].u),
                side ? GREEN : RED, 1 });
        }
    }
    for (int side = 0; side < 2; side++) {
        const int other = sidePick[1 - side];
        if (sidePick[side] >= 0 || other < 0 || std::fabs(found[other].t - tCentre) < 0.3f) { continue; }
        placed.push_back({ side ? ROLE_STARBOARD : ROLE_PORT,
            toModel(found[other].f, 2.0f * tCentre - found[other].t, found[other].u), side ? GREEN : RED, 1 });
    }
    //Lenses that were no sidelight are not lamps either
    for (size_t i = 0; i < found.size(); i++) { if (!found[i].strong) { found[i].used = true; } }
    //White: the highest is the masthead light, one right aft the sternlight, one forward the anchor light
    {
        int hi = -1;
        for (size_t i = 0; i < found.size(); i++) {
            if (found[i].used || found[i].colour != LC_WHITE) { continue; }
            if (hi < 0 || found[i].u > found[hi].u) { hi = (int)i; }
        }
        if (hi >= 0 && found[hi].u > 0.4f * uMax) {
            found[hi].used = true;
            placed.push_back({ ROLE_MASTHEAD, toModel(found[hi].f, found[hi].t, found[hi].u), WHITE, 1 });
        }
        for (size_t i = 0; i < found.size(); i++) {
            if (found[i].used || found[i].colour != LC_WHITE) { continue; }
            if (!have(ROLE_STERN) && found[i].f < fMin + 0.15f * span) {
                found[i].used = true;
                placed.push_back({ ROLE_STERN, toModel(found[i].f, found[i].t, found[i].u), WHITE, 1 });
            } else if (!have(ROLE_ANCHOR) && found[i].f > fMid) {
                found[i].used = true;
                placed.push_back({ ROLE_ANCHOR, toModel(found[i].f, found[i].t, found[i].u), WHITE, 1 });
            }
        }
    }
    //Anything else the modeller built is kept where it is, in its own colour
    for (size_t i = 0; i < found.size(); i++) {
        if (found[i].used) { continue; }
        const LampColour c = found[i].colour;
        placed.push_back({ c == LC_YELLOW ? ROLE_DECK : ROLE_CUSTOM, toModel(found[i].f, found[i].t, found[i].u),
            c == LC_RED ? RED : (c == LC_GREEN ? GREEN : (c == LC_YELLOW ? YELLOW : WHITE)), 1 });
    }
    const int fromModel = (int)placed.size() + (haveStack ? 1 : 0);

    //---- 2. the rest, on the hull's real shape -------------------------------------------------
    //The deck: the lower part of the centreline profile
    std::vector<float> profF, profU;
    for (int i = 0; i <= 160; i++) {
        const float f = fMin + 0.06f * span + 0.88f * span * i / 160.0f;
        float u;
        if (top(f, 0.0f, u) || top(f, 0.15f * halfBeam, u) || top(f, -0.15f * halfBeam, u)) { profF.push_back(f); profU.push_back(u); }
    }
    float deckU = 1.0f;
    if (!profU.empty()) {
        std::vector<float> sorted = profU;
        std::sort(sorted.begin(), sorted.end());
        deckU = sorted[sorted.size() * 3 / 10];
    }
    auto localDeck = [&](float f, float t) { //lowest top just around a point: the deck under a mast
        float best = 1e30f, u;
        const float d[4][2] = { { 1.5f, 0 }, { -1.5f, 0 }, { 0, 1.0f }, { 0, -1.0f } };
        for (int k = 0; k < 4; k++) { if (top(f + d[k][0], t + d[k][1], u)) { best = std::min(best, u); } }
        return (best < 1e29f) ? best : deckU;
    };

    //The mast: the highest point near the centreline (two masts from 50 m: forward and aft halves)
    struct Peak { bool ok; float f, t, u; };
    auto highest = [&](float f0, float f1) {
        Peak p = { false, 0, 0, -1e30f };
        const int nf = 150, nt = 14;
        for (int i = 0; i <= nf; i++) {
            const float f = f0 + (f1 - f0) * i / (float)nf;
            for (int j = 0; j <= nt; j++) {
                const float t = -0.35f * halfBeam + 0.7f * halfBeam * j / (float)nt + 0.5f * (tMin + tMax);
                float u;
                if (top(f, t, u) && u > p.u) { p.ok = true; p.f = f; p.t = t; p.u = u; }
            }
        }
        return p;
    };
    Peak mast = { false, 0, 0, 0 }, mastAft = { false, 0, 0, 0 };
    if (L >= 50.0f) {
        mast = highest(fMid - 0.05f * span, fMax - 0.05f * span);
        mastAft = highest(fMin + 0.05f * span, fMid - 0.05f * span);
    } else {
        mast = highest(fMin + 0.05f * span, fMax - 0.05f * span);
    }
    float mastLampU = 0.0f;
    for (size_t i = 0; i < placed.size(); i++) {
        if (placed[i].role == ROLE_MASTHEAD) {
            //the model's own masthead lamp shows where her mast is
            const ShipPoint sp = toShip(placed[i].pos);
            mastLampU = sp.v[2];
            mast.ok = true; mast.f = sp.v[0]; mast.t = sp.v[1]; mast.u = sp.v[2] - 0.15f;
        }
    }
    if (!have(ROLE_MASTHEAD) && mast.ok) {
        mastLampU = mast.u + 0.15f;
        placed.push_back({ ROLE_MASTHEAD, toModel(mast.f, mast.t, mastLampU), WHITE, 2 });
    }
    if (L >= 50.0f && !have(ROLE_MASTHEAD_AFT) && mastAft.ok) {
        placed.push_back({ ROLE_MASTHEAD_AFT, toModel(mastAft.f, mastAft.t, mastAft.u + 0.15f), WHITE, 2 });
    }

    //The signal line on the mast, under the masthead light if there is room, otherwise above it.
    //On the mast's forward face, so the lamps are not inside it.
    if (!haveStack && mast.ok) {
        const float sp = spacingRule;
        const float deckUnder = localDeck(mast.f, mast.t);
        float lowest = mastLampU - 3.0f * sp;
        if (lowest < deckUnder + stackLift) { lowest = mastLampU + sp; }
        float fFace = mast.f;
        {
            std::vector<float> h;
            rc.hits(0, mast.t, lowest + sp, h);
            float bestF = -1e30f;
            for (size_t k = 0; k < h.size(); k++) {
                if (std::fabs(h[k] - mast.f) < 1.5f && h[k] > bestF) { bestF = h[k]; }
            }
            if (bestF > -1e29f) { fFace = bestF + 0.08f; }
        }
        haveStack = true;
        stackSpacing = sp;
        stackBase = toModel(fFace, mast.t, lowest);
        stackOrigin = 2;
    }

    //The wheelhouse: the longest stretch of the profile standing well above the deck
    float houseA = 0, houseF = 0, houseTop = 0;
    bool house = false;
    {
        float bestLen = 0.0f;
        size_t i = 0;
        while (i < profF.size()) {
            if (profU[i] > deckU + 1.5f) {
                size_t j = i;
                float topU = profU[i];
                while (j + 1 < profF.size() && profU[j + 1] > deckU + 1.5f) { j++; topU = std::max(topU, profU[j]); }
                const float len = profF[j] - profF[i];
                if (len > bestLen) { bestLen = len; houseA = profF[i]; houseF = profF[j]; houseTop = topU; house = true; }
                i = j + 1;
            } else { i++; }
        }
        if (bestLen < 0.8f) { house = false; }
    }

    //Sidelights: at the sides of the wheelhouse front, half-way up it, not above 3/4 of the
    //masthead light; failing that, on the hull's side forward of amidships. Lower down the side
    //if the ray finds nothing there; one side found alone gives the other by symmetry.
    {
        float f = fMid + 0.15f * span, u = deckU + 0.6f;
        if (house) {
            f = houseF - std::min(0.5f, 0.2f * (houseF - houseA));
            u = deckU + 0.5f * (houseTop - deckU);
            if (u < deckU + 0.8f) { u = deckU + 0.8f; }
        }
        if (mastLampU > 0.0f && u > 0.75f * mastLampU) { u = 0.75f * mastLampU; }
        bool got[2] = { false, false };
        float gotT[2] = { 0, 0 }, gotU[2] = { u, u };
        for (int side = 0; side < 2; side++) {
            const float sgn = side ? 1.0f : -1.0f;
            for (float uu = u; uu > deckU + 0.2f && !got[side]; uu -= 0.4f) {
                std::vector<float> h;
                rc.hits(1, f, uu, h);
                for (size_t k = 0; k < h.size(); k++) {
                    if ((h[k] - tCentre) * sgn < 0.3f) { continue; } //the far side, or the centreline
                    if (!got[side] || (side ? h[k] > gotT[side] : h[k] < gotT[side])) { gotT[side] = h[k]; got[side] = true; gotU[side] = uu; }
                }
            }
        }
        for (int side = 0; side < 2; side++) {
            const Role role = side ? ROLE_STARBOARD : ROLE_PORT;
            if (have(role)) { continue; }
            const float sgn = side ? 1.0f : -1.0f;
            float t, uu;
            if (got[side]) { t = gotT[side]; uu = gotU[side]; }
            else if (got[1 - side]) { t = 2.0f * tCentre - gotT[1 - side]; uu = gotU[1 - side]; }
            else { t = tCentre + sgn * 0.9f * halfBeam; uu = deckU + 0.6f; }
            placed.push_back({ role, toModel(f, t + sgn * 0.08f, uu), side ? GREEN : RED, 2 });
        }
    }

    //Sternlight: as near the stern as can be, on the transom or the rail at the stern
    if (!have(ROLE_STERN)) {
        float bestU = -1.0f, bestF = fMin;
        for (float u = 0.3f; u <= std::min(uMax, deckU + 2.0f); u += 0.1f) {
            float f;
            if (rc.minHit(0, 0.5f * (tMin + tMax), u, f) && f < fMin + 0.12f * span) { bestU = u; bestF = f; }
        }
        if (bestU < 0.0f) { bestU = deckU + 0.5f; bestF = fMin + 0.03f * span; }
        placed.push_back({ ROLE_STERN, toModel(bestF - 0.08f, 0.5f * (tMin + tMax), bestU), WHITE, 2 });
    }

    //Anchor light: in the fore part, on top of the bow
    if (!have(ROLE_ANCHOR)) {
        float u = deckU + 1.0f, f = fMax - 0.04f * span;
        for (int i = 0; i < 40; i++) {
            const float ff = fMax - 0.02f * span - 0.004f * span * i;
            float uu;
            if (top(ff, 0.5f * (tMin + tMax), uu) && uu > 0.0f) { f = ff; u = uu; break; }
        }
        placed.push_back({ ROLE_ANCHOR, toModel(f, 0.5f * (tMin + tMax), u + 0.3f), WHITE, 2 });
    }

    //---- apply: the navigation lamps and the signal line are replaced, working lights stay ----
    for (size_t i = 0; i < lamps.size(); i++) {
        const Role r = lamps[i].role;
        if (r == ROLE_MASTHEAD || r == ROLE_MASTHEAD_AFT || r == ROLE_PORT || r == ROLE_STARBOARD ||
            r == ROLE_STERN || r == ROLE_ANCHOR || r == ROLE_SIGNAL) { removeLamp((int)i); }
    }
    signalLamps.clear();
    int fromRules = 0;
    for (size_t i = 0; i < placed.size(); i++) {
        const Placed& p = placed[i];
        if (p.role == ROLE_DECK || p.role == ROLE_CUSTOM) {
            //not twice the same working light
            bool dup = false;
            for (size_t k = 0; k < lamps.size(); k++) {
                if (lamps[k].light && lamps[k].light->getLocalPosition().getDistanceFrom(p.pos) < 0.5f * m) { dup = true; }
            }
            if (dup) { continue; }
        }
        addRoleLamp(p.role, p.pos, p.colour);
        lamps.back().origin = p.origin;
        if (p.origin == 2) { fromRules++; }
    }
    if (haveStack) {
        signalSpacingM = stackSpacing;
        addSignalStack(stackBase);
        for (size_t k = 0; k < signalLamps.size(); k++) { lamps[signalLamps[k]].origin = stackOrigin; }
        if (stackOrigin == 2) { fromRules++; }
    }
    {
        const size_t before = lamps.size();
        ensureAftAnchor();
        for (size_t k = before; k < lamps.size(); k++) { lamps[k].origin = 2; fromRules++; }
    }
    buildEditList();
    proposalApplied = true;
    editSel = 0;

    //---- what was done -------------------------------------------------------------------------
    wchar_t buf[512];
    std::swprintf(buf, 512, L"Proposition : %d feu(x) lu(s) dans le mod\u00E8le 3D, %d plac\u00E9(s) sur la forme de la coque "
        L"(COLREG Annexe I). M\u00E2t : feu \u00E0 %.1f m au-dessus de l'eau, pont \u00E0 %.1f m.",
        fromModel, fromRules, mastLampU, deckU);
    report = buf;
    if (haveStack && stackOrigin == 1 && stackSpacing < spacingRule) {
        std::swprintf(buf, 512, L" Feux de signal du mod\u00E8le espac\u00E9s de %.2f m (r\u00E8gle : %.0f m).", stackSpacing, spacingRule);
        report += buf;
    }
    {
        //Annex I 2 (a) (ii): the aft masthead light at least 4.5 m above the forward one
        float fwdU = -1.0f, aftU = -1.0f;
        for (size_t k = 0; k < lamps.size(); k++) {
            if (!lamps[k].light) { continue; }
            if (lamps[k].role == ROLE_MASTHEAD) { fwdU = toShip(lamps[k].light->getLocalPosition()).v[2]; }
            if (lamps[k].role == ROLE_MASTHEAD_AFT) { aftU = toShip(lamps[k].light->getLocalPosition()).v[2]; }
        }
        if (fwdU > 0.0f && aftU > 0.0f && aftU < fwdU + 4.5f) {
            report += L" Attention : le feu de t\u00EAte de m\u00E2t arri\u00E8re doit \u00EAtre au moins 4,5 m plus haut que l'avant (Annexe I) - \u00E0 ajuster.";
        }
    }
    if (!modelNote.empty()) { report += L" (" + modelNote + L")"; }
    report += L" V\u00E9rifiez chaque feu, puis Enregistrer - Annuler revient au boat.ini.";
    return true;
}
