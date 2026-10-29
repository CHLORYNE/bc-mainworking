/*   NAUTITECH - Simulateur de Navigation
     Frame timing (bc5.ini frame_timing=1): the time spent in each IPROF block, per frame on average,
     written to the log every 5 seconds with the frame rate, so a slow PC shows where its frames go.
     Off, each block costs one test of a flag.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __FRAMETIMING_HPP_INCLUDED__
#define __FRAMETIMING_HPP_INCLUDED__

#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace FrameTiming {

    struct Slot {
        const char* name;
        double seconds;
    };

    inline bool& enabled()
    {
        static bool on = false;
        return on;
    }

    inline std::vector<Slot>& slots()
    {
        static std::vector<Slot> list;
        return list;
    }

    inline void add(const char* name, double seconds)
    {
        std::vector<Slot>& list = slots();
        for (size_t i = 0; i < list.size(); i++) {
            if (list[i].name == name || strcmp(list[i].name, name) == 0) {
                list[i].seconds += seconds;
                return;
            }
        }
        Slot slot = { name, seconds };
        list.push_back(slot);
    }

    struct Scope {
        const char* name;
        bool on;
        std::chrono::steady_clock::time_point start;
        explicit Scope(const char* n) : name(n), on(enabled())
        {
            if (on) { start = std::chrono::steady_clock::now(); }
        }
        ~Scope()
        {
            if (on) { add(name, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()); }
        }
    };

    //Once a frame, at the end of the main loop
    inline void endFrame()
    {
        if (!enabled()) { return; }
        static unsigned frames = 0;
        static std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        frames++;
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        const double span = std::chrono::duration<double>(now - begin).count();
        if (span < 5.0 || frames == 0) { return; }
        char text[96];
        snprintf(text, sizeof(text), "FRAME TIMING %.1f fps, %.1f ms a frame. ms per frame:", frames / span, 1000.0 * span / frames);
        std::string line = text;
        std::vector<Slot>& list = slots();
        for (size_t i = 0; i < list.size(); i++) {
            snprintf(text, sizeof(text), " | %s %.2f", list[i].name, 1000.0 * list[i].seconds / frames);
            line += text;
            list[i].seconds = 0;
        }
        std::cerr << line << std::endl;
        frames = 0;
        begin = now;
    }
}

#define FRAMETIMING_CAT2(a, b) a##b
#define FRAMETIMING_CAT(a, b) FRAMETIMING_CAT2(a, b)
#define IPROF(a) FrameTiming::Scope FRAMETIMING_CAT(frameTimingScope, __LINE__)(a)

#endif
