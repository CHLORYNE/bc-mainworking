//Menu sounds of the launcher: a tick when the selection moves, a confirmation when an item is chosen,
//a sound for going back, a whoosh when the menu appears and one when an application is launched.
//Each is media/sounds/ui_<name>.wav if that file exists (PCM, 8 or 16 bit), else a built-in sound.
//Windows only (waveOut); silent elsewhere.

#ifndef __LAUNCHER_UISOUND_HPP_INCLUDED__
#define __LAUNCHER_UISOUND_HPP_INCLUDED__

#include "irrlicht.h"
#include <string>
#include <vector>

class UiSounds
{
public:
    enum Kind { Hover, Select, Back, Open, Launch, KindCount };

    UiSounds();
    ~UiSounds();

    void load();
    void play(Kind kind);
    void setEnabled(bool on) { enabled = on; }
    bool isEnabled() const { return enabled; }

    //The built-in sound: mono, 16 bit, 44100 Hz.
    static std::vector<irr::s16> synthesise(Kind kind);
    //The file a sound is read from, if present.
    static std::string fileName(Kind kind);

private:
    struct Voice;
    Voice* voices[KindCount];
    bool enabled;
    irr::u32 lastHoverMs;
};

#endif
