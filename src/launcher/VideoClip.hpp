//Video for the launcher: the intro film, and an optional looping menu background or menu music.
//The file is decoded on a thread of its own and its frames are shown in an Irrlicht texture.
//Windows: Media Foundation (MP4/MOV with H.264 and AAC, WMV, ...), sound through waveOut. Media
//Foundation is loaded when a clip is opened, so the launcher still starts on a PC without it.
//Elsewhere: ffmpeg, if it is installed (picture only).
//A file that is missing or cannot be decoded is simply not shown: failed() becomes true.

#ifndef __LAUNCHER_VIDEOCLIP_HPP_INCLUDED__
#define __LAUNCHER_VIDEOCLIP_HPP_INCLUDED__

#include "irrlicht.h"
#include <string>

class VideoClip
{
public:
    VideoClip();
    ~VideoClip();

    //base + the first of .mp4 .m4v .mov .wmv .avi .mkv that exists; "" if none.
    static std::string find(const std::string& base);
    //Audio files: base + .mp3 .m4a .wma .wav .aac.
    static std::string findAudio(const std::string& base);

    //Starts decoding (returns at once; failed() says later if the file could not be played).
    //picture: show the video track. sound: play the audio track. loop: start again at the end.
    bool open(const std::string& path, bool picture, bool sound, bool loop);

    //Once per frame, from the main thread: the texture with the picture due now (0 before the first).
    irr::video::ITexture* update(irr::video::IVideoDriver* driver);
    //Size of the picture (the texture may be larger).
    irr::core::dimension2du size() const;

    bool isOpen() const { return impl != 0; }
    bool started() const;     //playing (the first picture is shown)
    bool failed() const;      //could not be played
    bool finished() const;    //played to the end (not looping), or failed
    irr::f32 position() const;//seconds
    irr::f32 duration() const;//seconds, 0 if unknown

    //Why the clip could not be played ("" if it plays, or has not failed yet).
    std::string error() const;

    void setVolume(irr::f32 volume); //0..1
    void pause(bool on);
    void close();

    struct Impl;

private:
    Impl* impl;
};

#endif
