#include "UiSound.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {

const irr::f32 kRate = 44100.0f;
const irr::f32 kTwoPi = 6.28318530718f;

irr::u32 nowMs()
{
    return (irr::u32)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

#ifdef _WIN32
//A PCM wave file: format and samples. False if it is not one (or not 8/16 bit PCM).
bool readWave(const std::string& path, irr::u16& channels, irr::u32& rate, irr::u16& bits, std::vector<char>& data)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) { return false; }
    std::vector<char> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (file.size() < 44 || std::memcmp(&file[0], "RIFF", 4) != 0 || std::memcmp(&file[8], "WAVE", 4) != 0) { return false; }
    auto u16at = [&](size_t p) { return (irr::u16)((unsigned char)file[p] | ((unsigned char)file[p + 1] << 8)); };
    auto u32at = [&](size_t p) { return (irr::u32)u16at(p) | ((irr::u32)u16at(p + 2) << 16); };
    bool haveFormat = false;
    size_t p = 12;
    while (p + 8 <= file.size()) {
        const irr::u32 size = u32at(p + 4);
        const size_t body = p + 8;
        if (body + size > file.size()) { break; }
        if (std::memcmp(&file[p], "fmt ", 4) == 0 && size >= 16) {
            irr::u16 tag = u16at(body);
            channels = u16at(body + 2);
            rate = u32at(body + 4);
            bits = u16at(body + 14);
            if (tag == 0xFFFE && size >= 26) { tag = u16at(body + 24); } //WAVE_FORMAT_EXTENSIBLE: the sub-format
            haveFormat = (tag == 1) && (bits == 8 || bits == 16) && channels >= 1 && channels <= 2 && rate >= 8000;
        }
        else if (std::memcmp(&file[p], "data", 4) == 0 && haveFormat) {
            data.assign(file.begin() + body, file.begin() + body + size);
            return !data.empty();
        }
        p = body + size + (size & 1);
    }
    return false;
}
#endif

}

std::string UiSounds::fileName(Kind kind)
{
    static const char* names[KindCount] = { "hover", "select", "back", "open", "launch" };
    return std::string("media/sounds/ui_") + names[kind] + ".wav";
}

std::vector<irr::s16> UiSounds::synthesise(Kind kind)
{
    std::vector<irr::f32> s;
    irr::u32 seed = 0x2545F491u + (irr::u32)kind * 7919u;
    auto noise = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return ((seed >> 9) & 0x7FFF) / 16384.0f - 1.0f;
    };
    auto length = [&](irr::f32 seconds) { s.assign((size_t)(seconds * kRate), 0.0f); };
    auto ramp = [](irr::f32 t, irr::f32 attack) { return t < attack ? t / attack : 1.0f; };
    irr::f32 peak = 0.3f;

    switch (kind) {
    case Hover: {
        //A short glassy tick.
        length(0.07f);
        for (size_t i = 0; i < s.size(); i++) {
            const irr::f32 t = i / kRate;
            s[i] = (0.55f * std::sin(kTwoPi * 2350.0f * t) * std::exp(-t / 0.012f)
                + 0.25f * std::sin(kTwoPi * 4700.0f * t + 0.6f) * std::exp(-t / 0.006f)
                + 0.35f * noise() * std::exp(-t / 0.0015f)) * ramp(t, 0.001f);
        }
        peak = 0.2f;
        break;
    }
    case Select: {
        //Confirmation: a low thump, a bright two-note ping, a click on the attack.
        length(0.65f);
        irr::f32 phase = 0;
        for (size_t i = 0; i < s.size(); i++) {
            const irr::f32 t = i / kRate;
            phase += kTwoPi * (55.0f + 160.0f * std::exp(-t / 0.025f)) / kRate;
            irr::f32 v = 0.9f * std::sin(phase) * std::exp(-t / 0.09f);
            v += 0.28f * (std::sin(kTwoPi * 1180.0f * t) + 0.6f * std::sin(kTwoPi * 1770.0f * t) + 0.3f * std::sin(kTwoPi * 2360.0f * t))
                * std::exp(-t / 0.16f) * ramp(t, 0.012f);
            if (t > 0.06f) {
                const irr::f32 u = t - 0.06f;
                v += 0.22f * (std::sin(kTwoPi * 1575.0f * u) + 0.4f * std::sin(kTwoPi * 3150.0f * u)) * std::exp(-u / 0.2f) * ramp(u, 0.008f);
            }
            v += 0.5f * noise() * std::exp(-t / 0.002f);
            s[i] = v;
        }
        peak = 0.5f;
        break;
    }
    case Back: {
        //A falling blip.
        length(0.16f);
        irr::f32 phase = 0;
        for (size_t i = 0; i < s.size(); i++) {
            const irr::f32 t = i / kRate;
            phase += kTwoPi * (380.0f + 760.0f * std::exp(-t / 0.09f)) / kRate;
            s[i] = (std::sin(phase) + 0.3f * std::sin(2.0f * phase)) * std::exp(-t / 0.045f) * ramp(t, 0.002f);
        }
        peak = 0.28f;
        break;
    }
    case Open: {
        //The menu appears: an airy rising whoosh, a deep boom, a shimmer.
        length(1.9f);
        irr::f32 low = 0, boomPhase = 0;
        for (size_t i = 0; i < s.size(); i++) {
            const irr::f32 t = i / kRate;
            const irr::f32 cutoff = (t < 0.62f) ? 300.0f + 4700.0f * (t / 0.62f) * (t / 0.62f) : 800.0f + 4200.0f * std::exp(-(t - 0.62f) / 0.25f);
            low += (1.0f - std::exp(-kTwoPi * cutoff / kRate)) * (noise() - low);
            const irr::f32 rise = (t < 0.62f) ? (t / 0.62f) * (t / 0.62f) * (3.0f - 2.0f * t / 0.62f) : std::exp(-(t - 0.62f) / 0.35f);
            irr::f32 v = 0.9f * low * rise;
            if (t > 0.6f) {
                const irr::f32 u = t - 0.6f;
                boomPhase += kTwoPi * (46.0f + 34.0f * std::exp(-u / 0.05f)) / kRate;
                v += 0.95f * std::sin(boomPhase) * std::exp(-u / 0.38f) * ramp(u, 0.004f);
                v += 0.12f * (std::sin(kTwoPi * 2093.0f * u) + 0.7f * std::sin(kTwoPi * 3136.0f * u)) * std::exp(-u / 0.6f)
                    * (0.8f + 0.2f * std::sin(kTwoPi * 7.0f * u)) * ramp(u, 0.02f);
            }
            s[i] = v;
        }
        peak = 0.5f;
        break;
    }
    case Launch: {
        //An application starts: a rising tone and riser, then the confirmation.
        length(1.1f);
        irr::f32 phase = 0, low = 0, thump = 0;
        for (size_t i = 0; i < s.size(); i++) {
            const irr::f32 t = i / kRate;
            irr::f32 v = 0;
            if (t < 0.45f) {
                const irr::f32 k = t / 0.45f;
                phase += kTwoPi * (180.0f * std::pow(5.0f, k)) / kRate;
                low += (1.0f - std::exp(-kTwoPi * (600.0f + 5000.0f * k) / kRate)) * (noise() - low);
                v += (0.35f * std::sin(phase) + 0.12f * std::sin(2.0f * phase) + 0.5f * low) * k * k;
            }
            if (t > 0.42f) {
                const irr::f32 u = t - 0.42f;
                thump += kTwoPi * (50.0f + 150.0f * std::exp(-u / 0.03f)) / kRate;
                v += 0.9f * std::sin(thump) * std::exp(-u / 0.12f) * ramp(u, 0.003f);
                v += 0.25f * (std::sin(kTwoPi * 1320.0f * u) + 0.5f * std::sin(kTwoPi * 1980.0f * u) + 0.3f * std::sin(kTwoPi * 2640.0f * u))
                    * std::exp(-u / 0.25f) * ramp(u, 0.01f);
            }
            s[i] = v;
        }
        peak = 0.5f;
        break;
    }
    default:
        break;
    }

    irr::f32 highest = 0;
    for (size_t i = 0; i < s.size(); i++) { highest = std::max(highest, std::fabs(s[i])); }
    const irr::f32 gain = highest > 0 ? peak / highest : 0;
    const size_t fade = std::min(s.size(), (size_t)(0.005f * kRate));
    std::vector<irr::s16> out(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        irr::f32 v = s[i] * gain;
        if (i + fade >= s.size()) { v *= (irr::f32)(s.size() - i) / fade; }
        out[i] = (irr::s16)irr::core::clamp(v * 32767.0f, -32767.0f, 32767.0f);
    }
    return out;
}

#ifdef _WIN32

//One output stream per sound, so that different sounds can overlap; playing a sound again restarts it.
struct UiSounds::Voice
{
    HWAVEOUT out;
    WAVEHDR header;
    std::vector<char> data;

    Voice() : out(0) { std::memset(&header, 0, sizeof(header)); }

    bool open(irr::u16 channels, irr::u32 rate, irr::u16 bits)
    {
        WAVEFORMATEX format;
        std::memset(&format, 0, sizeof(format));
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = channels;
        format.nSamplesPerSec = rate;
        format.wBitsPerSample = bits;
        format.nBlockAlign = (WORD)(channels * bits / 8);
        format.nAvgBytesPerSec = rate * format.nBlockAlign;
        if (waveOutOpen(&out, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            out = 0;
            return false;
        }
        header.lpData = &data[0];
        header.dwBufferLength = (DWORD)data.size();
        if (waveOutPrepareHeader(out, &header, sizeof(header)) != MMSYSERR_NOERROR) {
            waveOutClose(out);
            out = 0;
            return false;
        }
        return true;
    }

    void play()
    {
        if (!out) { return; }
        waveOutReset(out);
        waveOutWrite(out, &header, sizeof(header));
    }

    ~Voice()
    {
        if (!out) { return; }
        waveOutReset(out);
        waveOutUnprepareHeader(out, &header, sizeof(header));
        waveOutClose(out);
    }
};

void UiSounds::load()
{
    for (int k = 0; k < KindCount; k++) {
        delete voices[k];
        voices[k] = 0;
        Voice* voice = new Voice();
        irr::u16 channels = 1, bits = 16;
        irr::u32 rate = 44100;
        if (!readWave(fileName((Kind)k), channels, rate, bits, voice->data)) {
            const std::vector<irr::s16> pcm = synthesise((Kind)k);
            voice->data.assign((const char*)&pcm[0], (const char*)&pcm[0] + pcm.size() * sizeof(irr::s16));
            channels = 1;
            rate = 44100;
            bits = 16;
        }
        if (voice->data.empty() || !voice->open(channels, rate, bits)) {
            delete voice;
            continue;
        }
        voices[k] = voice;
    }
}

#else

struct UiSounds::Voice
{
    void play() {}
};

void UiSounds::load()
{
}

#endif

UiSounds::UiSounds() : enabled(true), lastHoverMs(0)
{
    for (int k = 0; k < KindCount; k++) { voices[k] = 0; }
}

UiSounds::~UiSounds()
{
    for (int k = 0; k < KindCount; k++) { delete voices[k]; }
}

void UiSounds::play(Kind kind)
{
    if (!enabled || kind < 0 || kind >= KindCount || !voices[kind]) { return; }
    if (kind == Hover) {
        //Sweeping the mouse over the menu: not a tick for every item crossed in a few milliseconds.
        const irr::u32 now = nowMs();
        if (now - lastHoverMs < 40) { return; }
        lastHoverMs = now;
    }
    voices[kind]->play();
}
