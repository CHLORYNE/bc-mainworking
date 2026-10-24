//Media Foundation needs Windows 7 declarations; the rest of the launcher is built for older targets.
#ifdef _WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0601
#ifdef WINVER
#undef WINVER
#endif
#define WINVER 0x0601
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#endif

#include "VideoClip.hpp"
#include "../Utilities.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace {

typedef std::chrono::steady_clock Clock;

double secondsBetween(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double>(b - a).count();
}

const size_t kMaxFrames = 4;         //decoded pictures waiting to be shown
const double kMaxAudioAhead = 1.0;   //seconds of sound queued ahead of what is heard
const double kStartTimeout = 6.0;    //seconds to get the first picture before giving up

struct Frame {
    std::vector<irr::u32> pixels;
    double time;
};

}

#ifdef _WIN32

namespace {

//Media Foundation, from its DLLs (absent on some Windows editions: then there is no video).
struct MediaFoundation {
    typedef HRESULT(WINAPI* StartupFn)(ULONG, DWORD);
    typedef HRESULT(WINAPI* ShutdownFn)();
    typedef HRESULT(WINAPI* CreateAttributesFn)(IMFAttributes**, UINT32);
    typedef HRESULT(WINAPI* CreateMediaTypeFn)(IMFMediaType**);
    typedef HRESULT(WINAPI* CreateReaderFn)(LPCWSTR, IMFAttributes*, IMFSourceReader**);

    StartupFn startup;
    ShutdownFn shutdown;
    CreateAttributesFn createAttributes;
    CreateMediaTypeFn createMediaType;
    CreateReaderFn createReader;
    bool ok;

    MediaFoundation() : startup(0), shutdown(0), createAttributes(0), createMediaType(0), createReader(0), ok(false)
    {
        HMODULE plat = LoadLibraryA("mfplat.dll");
        HMODULE readwrite = LoadLibraryA("mfreadwrite.dll");
        if (!plat || !readwrite) { return; }
        startup = (StartupFn)(void*)GetProcAddress(plat, "MFStartup");
        shutdown = (ShutdownFn)(void*)GetProcAddress(plat, "MFShutdown");
        createAttributes = (CreateAttributesFn)(void*)GetProcAddress(plat, "MFCreateAttributes");
        createMediaType = (CreateMediaTypeFn)(void*)GetProcAddress(plat, "MFCreateMediaType");
        createReader = (CreateReaderFn)(void*)GetProcAddress(readwrite, "MFCreateSourceReaderFromURL");
        ok = startup && shutdown && createAttributes && createMediaType && createReader;
    }
};

MediaFoundation& mediaFoundation()
{
    static MediaFoundation api;
    return api;
}

//Media Foundation identifiers (here rather than from mfuuid.lib).
const GUID kNullGuid = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
const GUID kMajorType = { 0x48eba18e, 0xf8c9, 0x4687, { 0xbf, 0x11, 0x0a, 0x74, 0xc9, 0xf9, 0x6a, 0x8f } };
const GUID kSubtype = { 0xf7e34c9a, 0x42e8, 0x4714, { 0xb7, 0x4b, 0xcb, 0x29, 0xd7, 0x2c, 0x35, 0xe5 } };
const GUID kVideo = { 0x73646976, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID kAudio = { 0x73647561, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID kRgb32 = { 0x00000016, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
//Decoder outputs converted here when Media Foundation offers no RGB converter (Wine, some editions).
const GUID kNv12 = { 0x3231564E, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID kI420 = { 0x30323449, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID kIyuv = { 0x56555949, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID kYv12 = { 0x32315659, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID kYuy2 = { 0x32595559, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID kPcm = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID kFrameSize = { 0x1652c33d, 0xd6b2, 0x4012, { 0xb8, 0x34, 0x72, 0x03, 0x08, 0x49, 0xa3, 0x7d } };
const GUID kDefaultStride = { 0x644b4e48, 0x1e02, 0x4516, { 0xb0, 0xeb, 0xc0, 0x1c, 0xa9, 0xd4, 0x9a, 0xc6 } };
const GUID kDisplayAperture = { 0xd7388766, 0x18fe, 0x48c6, { 0xa1, 0x77, 0xee, 0x89, 0x48, 0x67, 0xc8, 0xc4 } };
const GUID kChannels = { 0x37e48bf5, 0x645e, 0x4c5b, { 0x89, 0xde, 0xad, 0xa9, 0xe2, 0x9b, 0x69, 0x6a } };
const GUID kSampleRate = { 0x5faeeae7, 0x0290, 0x4c31, { 0x9e, 0x8a, 0xc5, 0x34, 0xf6, 0x8d, 0x9d, 0xba } };
const GUID kBitsPerSample = { 0xf2deb57f, 0x40fa, 0x4764, { 0xaa, 0x33, 0xed, 0x4f, 0x2d, 0x1f, 0xf6, 0x69 } };
const GUID kDuration = { 0x6c990d33, 0xbb8e, 0x477a, { 0x85, 0x98, 0x0d, 0x5d, 0x96, 0xfc, 0xd8, 0x8a } };
const GUID kVideoProcessing = { 0xfb394f3d, 0xccf1, 0x42ee, { 0xbb, 0xb3, 0xf9, 0xb8, 0x45, 0xd5, 0x68, 0x1d } };
const IID kIID2DBuffer = { 0x7dc9d5f9, 0x9ed9, 0x44ec, { 0x9b, 0xbf, 0x06, 0x00, 0xbb, 0x58, 0x9f, 0xbb } };
const ULONG kMFVersion = 0x00020070; //Windows 7 and later

enum PictureLayout { Layout_RGB32, Layout_NV12, Layout_I420, Layout_YV12, Layout_YUY2 };

//Video YUV (limited range) to BGRA: BT.709 for HD pictures, BT.601 below.
inline irr::u32 yuvToBgra(int y, int u, int v, const int* k)
{
    const int c = (y - 16) * 298, d = u - 128, e = v - 128;
    int r = (c + k[0] * e + 128) >> 8;
    int g = (c - k[1] * d - k[2] * e + 128) >> 8;
    int b = (c + k[3] * d + 128) >> 8;
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return 0xFF000000u | ((irr::u32)r << 16) | ((irr::u32)g << 8) | (irr::u32)b;
}
const int kBt709[4] = { 459, 55, 136, 541 };
const int kBt601[4] = { 409, 100, 208, 516 };

//A block of sound handed to waveOut.
struct AudioBlock {
    WAVEHDR header;
    std::vector<char> data;
};

}

#endif

struct VideoClip::Impl
{
    std::string path;
    bool picture, sound, loop;

    //Shared with the decoding thread (under lock).
    std::mutex lock;
    std::condition_variable wake;
    std::deque<Frame*> frames;
    std::vector<Frame*> spare;
    int width, height;
    double length;
    bool stop, ready, failed, ended;
    std::string problem;
    std::thread worker;

    //Main thread.
    irr::video::IVideoDriver* driver;
    irr::video::ITexture* texture;
    std::atomic<bool> started;
    bool paused, shownAny;
    Clock::time_point opened, startWall, pauseWall;
    double now;

#ifdef _WIN32
    std::mutex audioLock;
    HWAVEOUT audioOut;
    std::deque<AudioBlock*> audioQueue;
    double audioRate, audioBytesPerSecond, audioWritten;
    bool audioDrained;
    irr::f32 volume;
#endif

    Impl() : picture(true), sound(true), loop(false), width(0), height(0), length(0), stop(false), ready(false), failed(false), ended(false),
        driver(0), texture(0), started(false), paused(false), shownAny(false), now(0)
#ifdef _WIN32
        , audioOut(0), audioRate(0), audioBytesPerSecond(0), audioWritten(0), audioDrained(true), volume(1.0f)
#endif
    {
    }

    ~Impl()
    {
        shutDown();
        for (size_t i = 0; i < frames.size(); i++) { delete frames[i]; }
        for (size_t i = 0; i < spare.size(); i++) { delete spare[i]; }
    }

    void shutDown()
    {
        {
            std::lock_guard<std::mutex> guard(lock);
            stop = true;
        }
        wake.notify_all();
        if (worker.joinable()) { worker.join(); }
        if (texture && driver) { driver->removeTexture(texture); }
        texture = 0;
    }

    //Decoding thread: a picture buffer to fill (waits while enough are queued). 0 when stopping.
    Frame* takeFrame(size_t pixels)
    {
        std::unique_lock<std::mutex> guard(lock);
        while (!stop && frames.size() >= kMaxFrames) {
            wake.wait_for(guard, std::chrono::milliseconds(20));
            if (frames.size() >= kMaxFrames) {
                guard.unlock();
                reapAudio();
                guard.lock();
            }
        }
        if (stop) { return 0; }
        Frame* f = 0;
        if (!spare.empty()) {
            f = spare.back();
            spare.pop_back();
        }
        else {
            f = new Frame();
        }
        f->pixels.resize(pixels);
        return f;
    }

    void queueFrame(Frame* f)
    {
        {
            std::lock_guard<std::mutex> guard(lock);
            frames.push_back(f);
        }
        wake.notify_all();
    }

    void note(const std::string& why)
    {
        std::lock_guard<std::mutex> guard(lock);
        if (problem.empty()) { problem = why; }
    }

#ifdef _WIN32
    static std::string hex(HRESULT hr)
    {
        char text[16];
        snprintf(text, sizeof(text), "0x%08lX", (unsigned long)hr);
        return text;
    }
#endif

    //Main thread: the playing time, in seconds.
    double clock()
    {
        if (!started) { return 0; }
        const Clock::time_point t = paused ? pauseWall : Clock::now();
        double wall = secondsBetween(startWall, t);
#ifdef _WIN32
        //While its sound plays, the clip follows the sound.
        const double heard = audioHeard();
        if (heard >= 0 && !paused) {
            startWall += std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(wall - heard));
            wall = heard;
        }
#endif
        return wall;
    }

    void start()
    {
        started = true;
        startWall = Clock::now();
#ifdef _WIN32
        std::lock_guard<std::mutex> guard(audioLock);
        if (audioOut && !paused) { waveOutRestart(audioOut); }
#endif
    }

    void upload(const Frame* f)
    {
        if (!driver || width <= 0 || height <= 0) { return; }
        if (!texture || texture->getOriginalSize() != irr::core::dimension2du((irr::u32)width, (irr::u32)height)) {
            if (texture) { driver->removeTexture(texture); }
            static int serial = 0;
            const bool mipMaps = driver->getTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS);
            const bool keep = driver->getTextureCreationFlag(irr::video::ETCF_ALLOW_MEMORY_COPY);
            driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, false);
            driver->setTextureCreationFlag(irr::video::ETCF_ALLOW_MEMORY_COPY, true); //no allocation for every picture
            texture = driver->addTexture(irr::core::dimension2du((irr::u32)width, (irr::u32)height),
                ("launcher-video-" + std::to_string(serial++)).c_str(), irr::video::ECF_A8R8G8B8);
            driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, mipMaps);
            driver->setTextureCreationFlag(irr::video::ETCF_ALLOW_MEMORY_COPY, keep);
            if (!texture) { return; }
        }
        irr::u8* out = (irr::u8*)texture->lock(irr::video::ETLM_WRITE_ONLY);
        if (!out) { return; }
        const irr::core::dimension2du size = texture->getSize();
        const irr::u32 pitch = texture->getPitch();
        if (size.Width == (irr::u32)width && size.Height == (irr::u32)height) {
            for (int y = 0; y < height; y++) { std::memcpy(out + (size_t)y * pitch, &f->pixels[(size_t)y * width], (size_t)width * 4); }
        }
        else {
            //Power-of-two texture: the picture stretched over it.
            for (irr::u32 y = 0; y < size.Height; y++) {
                irr::u32* line = (irr::u32*)(out + (size_t)y * pitch);
                const irr::u32* src = &f->pixels[(size_t)(y * height / size.Height) * width];
                for (irr::u32 x = 0; x < size.Width; x++) { line[x] = src[x * width / size.Width]; }
            }
        }
        texture->unlock();
    }

#ifdef _WIN32
    //Seconds of sound heard since the start; -1 when the sound does not drive the clock.
    double audioHeard()
    {
        std::lock_guard<std::mutex> guard(audioLock);
        if (!audioOut || audioDrained || audioRate <= 0) { return -1; }
        MMTIME position;
        position.wType = TIME_SAMPLES;
        if (waveOutGetPosition(audioOut, &position, sizeof(position)) != MMSYSERR_NOERROR) { return -1; }
        if (position.wType == TIME_SAMPLES) { return position.u.sample / audioRate; }
        if (position.wType == TIME_BYTES && audioBytesPerSecond > 0) { return position.u.cb / audioBytesPerSecond; }
        return -1;
    }

    //Sound blocks played: given back. Sets audioDrained when nothing is left to play.
    void reapAudio()
    {
        std::lock_guard<std::mutex> guard(audioLock);
        while (!audioQueue.empty() && (audioQueue.front()->header.dwFlags & WHDR_DONE)) {
            AudioBlock* block = audioQueue.front();
            audioQueue.pop_front();
            waveOutUnprepareHeader(audioOut, &block->header, sizeof(block->header));
            delete block;
        }
    }

    double audioAhead()
    {
        double played = 0;
        {
            std::lock_guard<std::mutex> guard(audioLock);
            if (!audioOut || audioBytesPerSecond <= 0) { return 0; }
            MMTIME position;
            position.wType = TIME_BYTES;
            if (waveOutGetPosition(audioOut, &position, sizeof(position)) == MMSYSERR_NOERROR) {
                if (position.wType == TIME_BYTES) { played = position.u.cb / audioBytesPerSecond; }
                else if (position.wType == TIME_SAMPLES && audioRate > 0) { played = position.u.sample / audioRate; }
            }
        }
        return audioWritten - played;
    }

    bool openAudio(IMFSourceReader* reader)
    {
        MediaFoundation& mf = mediaFoundation();
        reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
        IMFMediaType* type = 0;
        if (FAILED(mf.createMediaType(&type))) { return false; }
        type->SetGUID(kMajorType, kAudio);
        type->SetGUID(kSubtype, kPcm);
        type->SetUINT32(kBitsPerSample, 16);
        HRESULT hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, type);
        type->Release();
        IMFMediaType* current = 0;
        if (SUCCEEDED(hr)) { hr = reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &current); }
        if (FAILED(hr) || !current) {
            reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, FALSE);
            return false;
        }
        UINT32 channels = 2, rate = 44100, bits = 16;
        current->GetUINT32(kChannels, &channels);
        current->GetUINT32(kSampleRate, &rate);
        current->GetUINT32(kBitsPerSample, &bits);
        current->Release();

        WAVEFORMATEX format;
        std::memset(&format, 0, sizeof(format));
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = (WORD)channels;
        format.nSamplesPerSec = rate;
        format.wBitsPerSample = (WORD)bits;
        format.nBlockAlign = (WORD)(channels * bits / 8);
        format.nAvgBytesPerSec = rate * format.nBlockAlign;
        std::lock_guard<std::mutex> guard(audioLock);
        if (waveOutOpen(&audioOut, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            audioOut = 0;
            reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, FALSE);
            return false;
        }
        waveOutPause(audioOut); //until the first picture is shown
        const DWORD level = (DWORD)(irr::core::clamp(volume, 0.0f, 1.0f) * 0xFFFF);
        waveOutSetVolume(audioOut, level | (level << 16));
        audioRate = rate;
        audioBytesPerSecond = format.nAvgBytesPerSec;
        audioDrained = false;
        return true;
    }

    void queueAudio(IMFSample* sample)
    {
        IMFMediaBuffer* buffer = 0;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer)) || !buffer) { return; }
        BYTE* data = 0;
        DWORD maxLength = 0, length = 0;
        if (SUCCEEDED(buffer->Lock(&data, &maxLength, &length)) && length > 0) {
            AudioBlock* block = new AudioBlock();
            block->data.assign((const char*)data, (const char*)data + length);
            std::memset(&block->header, 0, sizeof(block->header));
            block->header.lpData = &block->data[0];
            block->header.dwBufferLength = length;
            std::lock_guard<std::mutex> guard(audioLock);
            if (waveOutPrepareHeader(audioOut, &block->header, sizeof(block->header)) == MMSYSERR_NOERROR
                && waveOutWrite(audioOut, &block->header, sizeof(block->header)) == MMSYSERR_NOERROR) {
                audioQueue.push_back(block);
                audioWritten += length / audioBytesPerSecond;
            }
            else {
                delete block;
            }
            buffer->Unlock();
        }
        buffer->Release();
    }

    void closeAudio()
    {
        std::lock_guard<std::mutex> guard(audioLock);
        if (!audioOut) { return; }
        waveOutReset(audioOut);
        while (!audioQueue.empty()) {
            waveOutUnprepareHeader(audioOut, &audioQueue.front()->header, sizeof(WAVEHDR));
            delete audioQueue.front();
            audioQueue.pop_front();
        }
        waveOutClose(audioOut);
        audioOut = 0;
        audioDrained = true;
    }

    //Picture format of the video stream: size, stride, the part to show.
    struct PictureFormat {
        int codedWidth, codedHeight, stride, left, top, width, height;
        PictureLayout layout;
    };

    bool readPictureFormat(IMFSourceReader* reader, PictureFormat& format)
    {
        IMFMediaType* current = 0;
        if (FAILED(reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &current)) || !current) { return false; }
        UINT64 size = 0;
        current->GetUINT64(kFrameSize, &size);
        format.codedWidth = (int)(size >> 32);
        format.codedHeight = (int)(size & 0xFFFFFFFF);
        GUID subtype = kRgb32;
        current->GetGUID(kSubtype, &subtype);
        format.layout = (subtype == kNv12) ? Layout_NV12 : (subtype == kI420 || subtype == kIyuv) ? Layout_I420
            : (subtype == kYv12) ? Layout_YV12 : (subtype == kYuy2) ? Layout_YUY2 : Layout_RGB32;
        const int bytesPerPixel = (format.layout == Layout_RGB32) ? 4 : (format.layout == Layout_YUY2) ? 2 : 1;
        UINT32 stride = 0;
        format.stride = SUCCEEDED(current->GetUINT32(kDefaultStride, &stride)) ? (int)(INT32)stride : format.codedWidth * bytesPerPixel;
        format.left = format.top = 0;
        format.width = format.codedWidth;
        format.height = format.codedHeight;
        MFVideoArea area;
        UINT32 got = 0;
        if (SUCCEEDED(current->GetBlob(kDisplayAperture, (UINT8*)&area, sizeof(area), &got)) && got == sizeof(area)) {
            const int l = area.OffsetX.value, t = area.OffsetY.value, w = (int)area.Area.cx, h = (int)area.Area.cy;
            if (l >= 0 && t >= 0 && w > 0 && h > 0 && l + w <= format.codedWidth && t + h <= format.codedHeight) {
                format.left = l;
                format.top = t;
                format.width = w;
                format.height = h;
            }
        }
        current->Release();
        return format.codedWidth > 0 && format.codedHeight > 0;
    }

    void queuePicture(IMFSample* sample, const PictureFormat& format, double time)
    {
        IMFMediaBuffer* buffer = 0;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer)) || !buffer) { return; }
        Frame* f = takeFrame((size_t)format.width * format.height);
        if (!f) {
            buffer->Release();
            return;
        }
        IMF2DBuffer* buffer2d = 0;
        BYTE* first = 0;
        LONG pitch = 0;
        BYTE* data = 0;
        bool locked2d = false, locked = false;
        if (SUCCEEDED(buffer->QueryInterface(kIID2DBuffer, (void**)&buffer2d)) && buffer2d && SUCCEEDED(buffer2d->Lock2D(&first, &pitch))) {
            locked2d = true;
        }
        else {
            DWORD maxLength = 0, length = 0;
            if (SUCCEEDED(buffer->Lock(&data, &maxLength, &length))) {
                locked = true;
                pitch = format.stride;
                first = (pitch < 0) ? data + (size_t)(-pitch) * (format.codedHeight - 1) : data;
                const bool planar = (format.layout != Layout_RGB32 && format.layout != Layout_YUY2);
                if (length < (DWORD)(std::abs(pitch) * format.codedHeight * (planar ? 3 : 2) / 2)) { first = 0; }
            }
        }
        if (first) {
            const int* k = (format.height >= 720) ? kBt709 : kBt601;
            const ptrdiff_t lumaPlane = (ptrdiff_t)pitch * format.codedHeight;
            for (int y = 0; y < format.height; y++) {
                const int row = format.top + y;
                irr::u32* dst = &f->pixels[(size_t)y * format.width];
                const BYTE* luma = first + (ptrdiff_t)row * pitch;
                switch (format.layout) {
                case Layout_RGB32: {
                    const irr::u32* src = (const irr::u32*)luma + format.left;
                    for (int x = 0; x < format.width; x++) { dst[x] = src[x] | 0xFF000000; }
                    break;
                }
                case Layout_NV12: {
                    const BYTE* chroma = first + lumaPlane + (ptrdiff_t)(row / 2) * pitch;
                    for (int x = 0; x < format.width; x++) {
                        const int c = ((format.left + x) / 2) * 2;
                        dst[x] = yuvToBgra(luma[format.left + x], chroma[c], chroma[c + 1], k);
                    }
                    break;
                }
                case Layout_I420: case Layout_YV12: {
                    const ptrdiff_t half = pitch / 2, plane = half * (format.codedHeight / 2);
                    const BYTE* first2 = first + lumaPlane + (ptrdiff_t)(row / 2) * half;
                    const BYTE* us = (format.layout == Layout_I420) ? first2 : first2 + plane;
                    const BYTE* vs = (format.layout == Layout_I420) ? first2 + plane : first2;
                    for (int x = 0; x < format.width; x++) {
                        const int c = (format.left + x) / 2;
                        dst[x] = yuvToBgra(luma[format.left + x], us[c], vs[c], k);
                    }
                    break;
                }
                case Layout_YUY2: {
                    for (int x = 0; x < format.width; x++) {
                        const BYTE* pair = luma + ((format.left + x) / 2) * 4;
                        dst[x] = yuvToBgra(((format.left + x) & 1) ? pair[2] : pair[0], pair[1], pair[3], k);
                    }
                    break;
                }
                }
            }
        }
        if (locked2d) { buffer2d->Unlock2D(); }
        if (buffer2d) { buffer2d->Release(); }
        if (locked) { buffer->Unlock(); }
        buffer->Release();
        if (!first) {
            std::lock_guard<std::mutex> guard(lock);
            spare.push_back(f);
            return;
        }
        f->time = time;
        queueFrame(f);
    }

    void run()
    {
        const bool com = SUCCEEDED(CoInitializeEx(0, COINIT_MULTITHREADED));
        MediaFoundation& mf = mediaFoundation();
        if (!mf.ok) { note("Media Foundation is not installed (mfplat.dll / mfreadwrite.dll)"); }
        bool mfStarted = mf.ok && SUCCEEDED(mf.startup(kMFVersion, MFSTARTUP_NOSOCKET));
        if (mf.ok && !mfStarted) { note("MFStartup failed"); }
        IMFSourceReader* reader = 0;
        if (mfStarted) {
            IMFAttributes* attributes = 0;
            if (SUCCEEDED(mf.createAttributes(&attributes, 1)) && attributes) {
                attributes->SetUINT32(kVideoProcessing, TRUE); //decoder output converted to RGB
            }
            wchar_t full[MAX_PATH * 2];
            std::wstring wide(path.size() + 1, L'\0');
            const int n = MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, &wide[0], (int)wide.size());
            wide.resize(n > 0 ? n - 1 : 0);
            const DWORD fullLength = GetFullPathNameW(wide.c_str(), MAX_PATH * 2, full, 0);
            const std::wstring url = (fullLength > 0 && fullLength < MAX_PATH * 2) ? std::wstring(full) : wide;
            const HRESULT opened = mf.createReader(url.c_str(), attributes, &reader);
            if (FAILED(opened)) {
                reader = 0;
                note("cannot open the file (" + hex(opened) + "): unknown container or codec?");
            }
            if (attributes) { attributes->Release(); }
        }

        PictureFormat format;
        std::memset(&format, 0, sizeof(format));
        bool video = false, audio = false;
        if (reader) {
            reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
            if (picture) {
                reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
                //RGB from Media Foundation if it can, else the decoder's own YUV, converted here.
                const GUID outputs[6] = { kRgb32, kNv12, kI420, kIyuv, kYv12, kYuy2 };
                HRESULT firstError = S_OK;
                for (int i = 0; i < 6 && !video; i++) {
                    IMFMediaType* type = 0;
                    if (FAILED(mf.createMediaType(&type))) { break; }
                    type->SetGUID(kMajorType, kVideo);
                    type->SetGUID(kSubtype, outputs[i]);
                    const HRESULT set = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, type);
                    if (i == 0) { firstError = set; }
                    video = SUCCEEDED(set) && readPictureFormat(reader, format);
                    type->Release();
                }
                if (!video) { note("no decoder for the picture (" + hex(firstError) + "): use H.264 video in an MP4 file"); }
                if (!video) { reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, FALSE); }
            }
            if (sound) { audio = openAudio(reader); }
            PROPVARIANT value;
            PropVariantInit(&value);
            if (SUCCEEDED(reader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, kDuration, &value)) && value.vt == VT_UI8) {
                length = value.uhVal.QuadPart / 1e7;
            }
            PropVariantClear(&value);
        }
        const bool usable = picture ? video : audio;
        {
            std::lock_guard<std::mutex> guard(lock);
            width = format.width;
            height = format.height;
            ready = usable;
            failed = !usable;
            if (!usable && problem.empty()) { problem = picture ? "no picture track" : "no sound track"; }
        }
        wake.notify_all();

        if (usable) {
            double lastVideo = -1, lastAudio = -1, offset = 0, step = 1.0 / 30;
            bool videoEnded = !video, audioEnded = !audio;
            bool any = false;
            while (true) {
                {
                    std::lock_guard<std::mutex> guard(lock);
                    if (stop) { break; }
                }
                reapAudio();
                if (videoEnded && audioEnded) {
                    if (loop && any) {
                        //From the start again, the times carrying on.
                        PROPVARIANT start;
                        PropVariantInit(&start);
                        start.vt = VT_I8;
                        start.hVal.QuadPart = 0;
                        reader->SetCurrentPosition(kNullGuid, start);
                        offset = std::max(lastVideo, lastAudio) + step;
                        videoEnded = !video;
                        audioEnded = !audio;
                        any = false;
                        continue;
                    }
                    break;
                }
                const bool readVideo = !videoEnded && (audioEnded || lastVideo <= lastAudio);
                if (!readVideo && audioAhead() > kMaxAudioAhead && started) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                const DWORD stream = readVideo ? (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM : (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM;
                DWORD actual = 0, flags = 0;
                LONGLONG timestamp = 0;
                IMFSample* sample = 0;
                const HRESULT hr = reader->ReadSample(stream, 0, &actual, &flags, &timestamp, &sample);
                if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR)) {
                    note(std::string("decoding stopped (") + hex(hr) + (readVideo ? ", picture)" : ", sound)"));
                    if (readVideo) { videoEnded = true; } else { audioEnded = true; }
                    if (sample) { sample->Release(); }
                    continue;
                }
                if (readVideo && (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)) {
                    PictureFormat changed;
                    if (readPictureFormat(reader, changed)) {
                        format = changed;
                        std::lock_guard<std::mutex> guard(lock);
                        width = format.width;
                        height = format.height;
                    }
                }
                const double time = timestamp / 1e7 + offset;
                if (sample) {
                    any = true;
                    if (readVideo) {
                        queuePicture(sample, format, time);
                        if (lastVideo >= 0 && time > lastVideo) { step = std::min(std::max(time - lastVideo, 1.0 / 120), 0.1); }
                        lastVideo = time;
                    }
                    else {
                        queueAudio(sample);
                        lastAudio = time;
                    }
                    sample->Release();
                }
                else if (flags & MF_SOURCE_READERF_STREAMTICK) {
                    if (readVideo) { lastVideo = std::max(lastVideo, time); } else { lastAudio = std::max(lastAudio, time); }
                }
                if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
                    if (readVideo) { videoEnded = true; } else { audioEnded = true; }
                }
            }
            {
                std::lock_guard<std::mutex> guard(lock);
                ended = !stop;
            }
            wake.notify_all();
            //Let the last of the sound play, then say so (the clip is finished once it has).
            while (audio) {
                {
                    std::lock_guard<std::mutex> guard(lock);
                    if (stop) { break; }
                }
                reapAudio();
                {
                    std::lock_guard<std::mutex> guard(audioLock);
                    if (audioQueue.empty()) {
                        audioDrained = true;
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        closeAudio();
        if (reader) { reader->Release(); }
        if (mfStarted) { mf.shutdown(); }
        if (com) { CoUninitialize(); }
    }

#else

    void reapAudio() {}

    //ffmpeg on the path: the picture as raw BGRA through a pipe.
    static std::string quote(const std::string& s)
    {
        std::string out = "'";
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '\'') { out += "'\\''"; } else { out += s[i]; }
        }
        return out + "'";
    }

    void run()
    {
        int w = 0, h = 0;
        double fps = 0, probedLength = 0;
        if (picture) {
            const std::string probe = "ffprobe -v error -select_streams v:0 -show_entries stream=width,height,avg_frame_rate:format=duration -of default=nw=1 "
                + quote(path) + " 2>/dev/null";
            FILE* in = popen(probe.c_str(), "r");
            if (in) {
                char line[256];
                while (fgets(line, sizeof(line), in)) {
                    int num = 0, den = 0;
                    if (sscanf(line, "width=%d", &w) == 1 || sscanf(line, "height=%d", &h) == 1) { continue; }
                    if (sscanf(line, "avg_frame_rate=%d/%d", &num, &den) == 2 && den > 0) { fps = (double)num / den; continue; }
                    sscanf(line, "duration=%lf", &probedLength);
                }
                pclose(in);
            }
        }
        if (fps <= 0 || fps > 240) { fps = 30; }
        FILE* pipe = 0;
        if (w > 0 && h > 0) {
            const std::string command = std::string("ffmpeg -nostdin -v error ") + (loop ? "-stream_loop -1 " : "") + "-i " + quote(path)
                + " -an -f rawvideo -pix_fmt bgra - 2>/dev/null";
            pipe = popen(command.c_str(), "r");
        }
        {
            std::lock_guard<std::mutex> guard(lock);
            width = w;
            height = h;
            length = probedLength;
            ready = (pipe != 0);
            failed = (pipe == 0);
            if (!pipe) { problem = (w > 0) ? "ffmpeg could not be started" : "ffprobe could not read the file (is ffmpeg installed?)"; }
        }
        wake.notify_all();
        if (!pipe) { return; }
        const size_t pixels = (size_t)w * h;
        for (long n = 0;; n++) {
            Frame* f = takeFrame(pixels);
            if (!f) { break; }
            if (fread(&f->pixels[0], 4, pixels, pipe) != pixels) {
                std::lock_guard<std::mutex> guard(lock);
                spare.push_back(f);
                ended = true;
                break;
            }
            for (size_t i = 0; i < pixels; i++) { f->pixels[i] |= 0xFF000000; }
            f->time = n / fps;
            queueFrame(f);
        }
        wake.notify_all();
        pclose(pipe);
    }

#endif
};

VideoClip::VideoClip() : impl(0)
{
}

VideoClip::~VideoClip()
{
    close();
}

std::string VideoClip::find(const std::string& base)
{
    const char* extensions[] = { ".mp4", ".m4v", ".mov", ".wmv", ".avi", ".mkv", ".MP4", ".MOV" };
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        if (Utilities::pathExists(base + extensions[i])) { return base + extensions[i]; }
    }
    return "";
}

std::string VideoClip::findAudio(const std::string& base)
{
    const char* extensions[] = { ".mp3", ".m4a", ".wma", ".wav", ".aac", ".MP3" };
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        if (Utilities::pathExists(base + extensions[i])) { return base + extensions[i]; }
    }
    return "";
}

bool VideoClip::open(const std::string& path, bool picture, bool sound, bool loop)
{
    close();
    if (path.empty() || !Utilities::pathExists(path)) { return false; }
#ifndef _WIN32
    if (!picture) { return false; } //sound only through Media Foundation
#endif
    impl = new Impl();
    impl->path = path;
    impl->picture = picture;
    impl->sound = sound;
    impl->loop = loop;
    impl->opened = Clock::now();
    impl->worker = std::thread(&Impl::run, impl);
    return true;
}

irr::video::ITexture* VideoClip::update(irr::video::IVideoDriver* driver)
{
    if (!impl) { return 0; }
    impl->driver = driver;
    Frame* show = 0;
    {
        std::lock_guard<std::mutex> guard(impl->lock);
        if (impl->failed) { return impl->texture; }
        if (!impl->started) {
            const bool due = impl->picture ? !impl->frames.empty() : impl->ready;
            if (!due) {
                if (secondsBetween(impl->opened, Clock::now()) > kStartTimeout) {
                    impl->failed = true;
                    if (impl->problem.empty()) { impl->problem = "no picture decoded within 6 seconds"; }
                }
                return impl->texture;
            }
        }
    }
    if (!impl->started) { impl->start(); }
    impl->now = impl->clock();
    {
        std::lock_guard<std::mutex> guard(impl->lock);
        while (!impl->frames.empty() && (impl->frames.front()->time <= impl->now + 0.004 || !impl->shownAny)) {
            if (show) { impl->spare.push_back(show); }
            show = impl->frames.front();
            impl->frames.pop_front();
            if (!impl->shownAny) {
                impl->shownAny = true;
                break;
            }
        }
    }
    if (show) {
        impl->upload(show);
        {
            std::lock_guard<std::mutex> guard(impl->lock);
            impl->spare.push_back(show);
        }
        impl->wake.notify_all();
    }
    return impl->texture;
}

std::string VideoClip::error() const
{
    if (!impl) { return "not opened"; }
    std::lock_guard<std::mutex> guard(impl->lock);
    return impl->problem;
}

irr::core::dimension2du VideoClip::size() const
{
    if (!impl) { return irr::core::dimension2du(0, 0); }
    std::lock_guard<std::mutex> guard(impl->lock);
    return irr::core::dimension2du((irr::u32)std::max(0, impl->width), (irr::u32)std::max(0, impl->height));
}

bool VideoClip::started() const
{
    return impl && impl->started && (impl->shownAny || !impl->picture);
}

bool VideoClip::failed() const
{
    if (!impl) { return true; }
    std::lock_guard<std::mutex> guard(impl->lock);
    return impl->failed;
}

bool VideoClip::finished() const
{
    if (!impl) { return true; }
    bool done = false;
    {
        std::lock_guard<std::mutex> guard(impl->lock);
        if (impl->failed) { return true; }
        done = impl->ended && impl->frames.empty();
    }
#ifdef _WIN32
    if (done) {
        std::lock_guard<std::mutex> guard(impl->audioLock);
        done = impl->audioDrained;
    }
#endif
    return done;
}

irr::f32 VideoClip::position() const
{
    return impl ? (irr::f32)impl->now : 0.0f;
}

irr::f32 VideoClip::duration() const
{
    if (!impl) { return 0; }
    std::lock_guard<std::mutex> guard(impl->lock);
    return (irr::f32)impl->length;
}

void VideoClip::setVolume(irr::f32 volume)
{
#ifdef _WIN32
    if (!impl) { return; }
    std::lock_guard<std::mutex> guard(impl->audioLock);
    impl->volume = volume;
    if (impl->audioOut) {
        const DWORD level = (DWORD)(irr::core::clamp(volume, 0.0f, 1.0f) * 0xFFFF);
        waveOutSetVolume(impl->audioOut, level | (level << 16));
    }
#else
    (void)volume;
#endif
}

void VideoClip::pause(bool on)
{
    if (!impl || impl->paused == on) { return; }
    if (on) {
        impl->pauseWall = Clock::now();
    }
    else if (impl->started) {
        impl->startWall += Clock::now() - impl->pauseWall;
    }
    impl->paused = on;
#ifdef _WIN32
    std::lock_guard<std::mutex> guard(impl->audioLock);
    if (impl->audioOut && impl->started) {
        if (on) { waveOutPause(impl->audioOut); } else { waveOutRestart(impl->audioOut); }
    }
#endif
}

void VideoClip::close()
{
    delete impl;
    impl = 0;
}
