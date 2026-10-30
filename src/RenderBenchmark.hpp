/*   NAUTITECH - Simulateur de Navigation
     Render benchmark (bc5.ini render_benchmark=1): once, some seconds into an exercise, each part of
     the scene is hidden in turn for about a second - the terrain, the sea, the particles, the sky,
     each 3D model - and the time the 3D view takes without it is compared with the time it takes with
     everything shown. What each part costs on this PC is written to the log, the dearest first.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __RENDERBENCHMARK_HPP_INCLUDED__
#define __RENDERBENCHMARK_HPP_INCLUDED__

#include "irrlicht.h"
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

class RenderBenchmark
{
public:
    explicit RenderBenchmark(irr::f64 startAfterSeconds = 25.0) : startAfter(startAfterSeconds) {}

    //Once a frame, before the 3D view is drawn. secondsRunning: since the exercise began.
    void beforeRender(irr::scene::ISceneManager* smgr, irr::f64 secondsRunning)
    {
        if (state == Done || !smgr) { return; }
        if (state == Waiting) {
            if (secondsRunning < startAfter) { return; }
            collect(smgr);
            state = Running;
            phase = 0;
            frame = 0;
            std::cerr << "RENDER BENCHMARK: " << groups.size() << " parts of the scene, about " << (int)((groups.size() + 2) * kFrames / 20.0)
                << " s at 20 fps. The view flickers while it runs." << std::endl;
        }
        //Phase 0 and the last one: everything shown. Phase k (1..n): group k-1 hidden.
        if (frame == 0) { apply(); }
    }

    //Once a frame, after the 3D view: how long it took, ms
    void afterRender(irr::f64 ms)
    {
        if (state != Running) { return; }
        if (frame >= kSettle) { sum += ms; counted++; }
        frame++;
        if (frame < kSettle + kFrames) { return; }
        const irr::f64 average = counted ? sum / counted : 0.0;
        if (phase == 0) { baseline = average; }
        else if (phase <= (int)groups.size()) { groups[phase - 1].without = average; }
        else { baselineEnd = average; }
        sum = 0;
        counted = 0;
        frame = 0;
        phase++;
        if (phase > (int)groups.size() + 1) { finish(); }
    }

private:
    enum State { Waiting, Running, Done };
    struct Group {
        std::string name;
        std::vector<irr::scene::ISceneNode*> nodes;
        std::vector<bool> hidden;    //hidden by us in this phase (it was visible)
        irr::u32 triangles = 0, buffers = 0, count = 0;
        irr::f64 without = 0;
    };
    static const int kSettle = 8, kFrames = 24;

    State state = Waiting;
    irr::f64 startAfter;
    int phase = 0, frame = 0, counted = 0;
    irr::f64 sum = 0, baseline = 0, baselineEnd = 0;
    std::vector<Group> groups;

    static void meshSize(const irr::scene::IMesh* mesh, irr::u32& triangles, irr::u32& buffers)
    {
        triangles = buffers = 0;
        if (!mesh) { return; }
        buffers = mesh->getMeshBufferCount();
        for (irr::u32 b = 0; b < buffers; b++) { triangles += mesh->getMeshBuffer(b)->getIndexCount() / 3; }
    }

    static std::string typeName(irr::scene::ESCENE_NODE_TYPE t)
    {
        char c[5] = { (char)(t & 0xFF), (char)((t >> 8) & 0xFF), (char)((t >> 16) & 0xFF), (char)((t >> 24) & 0xFF), 0 };
        switch (t) {
        case irr::scene::ESNT_TERRAIN: return "terrain";
        case irr::scene::ESNT_PARTICLE_SYSTEM: return "particles (fire, smoke, spray, water jet)";
        case irr::scene::ESNT_BILLBOARD: return "billboards (lights, haloes)";
        case irr::scene::ESNT_SKY_DOME: case irr::scene::ESNT_SKY_BOX: return "sky";
        case irr::scene::ESNT_LIGHT: return "light sources";
        case irr::scene::ESNT_TEXT: case irr::scene::ESNT_BILLBOARD_TEXT: return "3D text";
        case irr::scene::ESNT_SPHERE: case irr::scene::ESNT_CUBE: return "spheres and cubes (lamps)";
        case irr::scene::ESNT_WATER_SURFACE: return "sea (with its reflections)";
        default:
            return std::string("nodes of type '") + c + "'";
        }
    }

    //Every visible node below the root (not the cameras, not under another node of the same
    //group), grouped: one group per 3D model, one per kind of other node
    void collect(irr::scene::ISceneManager* smgr)
    {
        std::map<std::string, size_t> index;
        std::vector<irr::scene::ISceneNode*> todo;
        const irr::core::list<irr::scene::ISceneNode*>& top = smgr->getRootSceneNode()->getChildren();
        for (irr::core::list<irr::scene::ISceneNode*>::ConstIterator it = top.begin(); it != top.end(); ++it) { todo.push_back(*it); }
        while (!todo.empty()) {
            irr::scene::ISceneNode* n = todo.back();
            todo.pop_back();
            if (!n || !n->isVisible()) { continue; }
            const irr::scene::ESCENE_NODE_TYPE t = n->getType();
            std::string key;
            irr::u32 tris = 0, bufs = 0;
            if (t == irr::scene::ESNT_CAMERA) { continue; }
            if (t == irr::scene::ESNT_MESH || t == irr::scene::ESNT_OCTREE || t == irr::scene::ESNT_ANIMATED_MESH) {
                const irr::scene::IMesh* mesh = 0;
                if (t == irr::scene::ESNT_ANIMATED_MESH) {
                    irr::scene::IAnimatedMesh* am = ((irr::scene::IAnimatedMeshSceneNode*)n)->getMesh();
                    mesh = am ? am->getMesh(0) : 0;
                    if (am) { key = smgr->getMeshCache()->getMeshName(am).getPath().c_str(); }
                }
                else {
                    mesh = ((irr::scene::IMeshSceneNode*)n)->getMesh();
                    if (mesh) { key = smgr->getMeshCache()->getMeshName(mesh).getPath().c_str(); }
                }
                meshSize(mesh, tris, bufs);
                if (key.empty()) {
                    char b[64];
                    snprintf(b, sizeof(b), "unnamed mesh, %u triangles", tris);
                    key = b;
                }
            }
            else {
                key = typeName(t);
            }
            size_t g;
            std::map<std::string, size_t>::iterator found = index.find(key);
            if (found == index.end()) {
                g = groups.size();
                index[key] = g;
                groups.push_back(Group());
                groups[g].name = key;
            }
            else {
                g = found->second;
            }
            n->grab(); //kept alive if the exercise removes it meanwhile (a fire put out, a raft picked up)
            groups[g].nodes.push_back(n);
            groups[g].count++;
            groups[g].triangles += tris;
            groups[g].buffers += bufs;
            //Children are hidden with their parent; still look inside for other kinds of node
            const irr::core::list<irr::scene::ISceneNode*>& kids = n->getChildren();
            for (irr::core::list<irr::scene::ISceneNode*>::ConstIterator it = kids.begin(); it != kids.end(); ++it) { todo.push_back(*it); }
        }
        //The biggest parts are measured; small ones of a kind together would be noise
        std::sort(groups.begin(), groups.end(), [](const Group& a, const Group& b) {
            return a.buffers * 4 + a.triangles / 1000 + a.nodes.size() > b.buffers * 4 + b.triangles / 1000 + b.nodes.size();
        });
        if (groups.size() > 24) {
            for (size_t g = 24; g < groups.size(); g++) {
                for (size_t i = 0; i < groups[g].nodes.size(); i++) { groups[g].nodes[i]->drop(); }
            }
            groups.resize(24);
        }
        for (size_t g = 0; g < groups.size(); g++) { groups[g].hidden.assign(groups[g].nodes.size(), false); }
    }

    void restore(Group& g)
    {
        for (size_t i = 0; i < g.nodes.size(); i++) {
            if (g.hidden[i]) { g.nodes[i]->setVisible(true); g.hidden[i] = false; }
        }
    }

    void apply()
    {
        for (size_t g = 0; g < groups.size(); g++) { restore(groups[g]); }
        if (phase >= 1 && phase <= (int)groups.size()) {
            Group& g = groups[phase - 1];
            for (size_t i = 0; i < g.nodes.size(); i++) {
                if (g.nodes[i]->isVisible()) { g.nodes[i]->setVisible(false); g.hidden[i] = true; }
            }
        }
    }

    void finish()
    {
        for (size_t g = 0; g < groups.size(); g++) { restore(groups[g]); }
        state = Done;
        for (size_t g = 0; g < groups.size(); g++) {
            for (size_t i = 0; i < groups[g].nodes.size(); i++) { groups[g].nodes[i]->drop(); }
            groups[g].nodes.clear();
        }
        std::vector<Group> sorted = groups;
        std::sort(sorted.begin(), sorted.end(), [&](const Group& a, const Group& b) { return a.without < b.without; });
        const irr::f64 base = 0.5 * (baseline + baselineEnd);
        char line[160];
        snprintf(line, sizeof(line), "RENDER BENCHMARK: the 3D view takes %.1f ms a frame (%.1f at the start, %.1f at the end). Without each part:",
            base, baseline, baselineEnd);
        std::cerr << line << std::endl;
        for (size_t g = 0; g < sorted.size(); g++) {
            snprintf(line, sizeof(line), "  %6.1f ms saved  (%u nodes, %u mesh buffers, %u triangles)  ",
                base - sorted[g].without, sorted[g].count, sorted[g].buffers, sorted[g].triangles);
            std::cerr << line << sorted[g].name << std::endl;
        }
        std::cerr << "RENDER BENCHMARK: end" << std::endl;
    }
};

#endif
