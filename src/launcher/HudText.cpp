#include "HudText.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>

//stb_truetype (public domain / MIT): its own warnings are not ours to fix.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244 4245 4456 4457 4701 4703 4996)
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "../libs/stb/stb_truetype.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace {

//TrueType files, read once and kept: the faces point into them.
const unsigned char* fontFile(const std::string& path)
{
    static std::map<std::string, std::unique_ptr<std::vector<unsigned char> > > files;
    std::map<std::string, std::unique_ptr<std::vector<unsigned char> > >::iterator it = files.find(path);
    if (it != files.end()) { return it->second ? &(*it->second)[0] : 0; }
    std::unique_ptr<std::vector<unsigned char> > data;
    std::ifstream in(path.c_str(), std::ios::binary);
    if (in) {
        data.reset(new std::vector<unsigned char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
        if (data->size() < 64) { data.reset(); }
    }
    const unsigned char* bytes = data ? &(*data)[0] : 0;
    files[path] = std::move(data);
    return bytes;
}

irr::u32 nextPowerOfTwo(irr::u32 v)
{
    irr::u32 p = 1;
    while (p < v) { p <<= 1; }
    return p;
}

}

HudFont::HudFont(irr::video::IVideoDriver* driver, const std::string& file, irr::f32 pixels, irr::gui::IGUIFont* fallback)
    : driver(driver), fallback(fallback), texture(0), face(0), scale(1), ascent(0), descent(0), caps(0)
{
    if (fallback) {
        const irr::f32 h = (irr::f32)fallback->getDimension(L"Hg").Height;
        ascent = h * 0.8f;
        descent = -h * 0.2f;
        caps = h * 0.56f;
    }
    const unsigned char* data = fontFile(file);
    if (!data || !driver || pixels < 4) { return; }
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, data, stbtt_GetFontOffsetForIndex(data, 0))) {
        delete info;
        return;
    }
    scale = stbtt_ScaleForPixelHeight(info, pixels);
    int a = 0, d = 0, gap = 0;
    stbtt_GetFontVMetrics(info, &a, &d, &gap);
    int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;

    //The characters: Latin-1, and the punctuation and arrows the launcher uses.
    std::vector<int> codes;
    for (int c = 32; c < 127; c++) { codes.push_back(c); }
    for (int c = 160; c < 256; c++) { codes.push_back(c); }
    const int extra[] = { 0x152, 0x153, 0x178, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x20AC, 0x2122,
        0x2190, 0x2191, 0x2192, 0x2193 };
    for (size_t i = 0; i < sizeof(extra) / sizeof(extra[0]); i++) { codes.push_back(extra[i]); }

    //Shelf packing into a power-of-two atlas.
    struct Box { int code, index, x0, y0, w, h, ax, ay; };
    std::vector<Box> boxes;
    irr::u32 area = 0;
    for (size_t i = 0; i < codes.size(); i++) {
        const int index = stbtt_FindGlyphIndex(info, codes[i]);
        if (index == 0 && codes[i] != ' ') { continue; }
        Box b;
        b.code = codes[i];
        b.index = index;
        stbtt_GetGlyphBitmapBox(info, index, scale, scale, &bx0, &by0, &bx1, &by1);
        b.x0 = bx0;
        b.y0 = by0;
        b.w = bx1 - bx0;
        b.h = by1 - by0;
        b.ax = b.ay = 0;
        area += (irr::u32)((b.w + 2) * (b.h + 2));
        boxes.push_back(b);
    }
    irr::u32 atlasW = irr::core::clamp(nextPowerOfTwo((irr::u32)(std::sqrt((double)area) * 1.25)), 128u, 4096u);
    irr::u32 atlasH = 0;
    for (int attempt = 0; attempt < 4; attempt++) {
        int x = 1, y = 1, row = 0;
        for (size_t i = 0; i < boxes.size(); i++) {
            if (x + boxes[i].w + 1 > (int)atlasW) { x = 1; y += row + 1; row = 0; }
            boxes[i].ax = x;
            boxes[i].ay = y;
            x += boxes[i].w + 1;
            row = std::max(row, boxes[i].h);
        }
        atlasH = nextPowerOfTwo((irr::u32)(y + row + 1));
        if (atlasH <= 4096) { break; }
        atlasW = std::min(atlasW * 2, 4096u);
    }
    if (atlasH > 4096) {
        delete info;
        return;
    }

    irr::video::IImage* image = driver->createImage(irr::video::ECF_A8R8G8B8, irr::core::dimension2du(atlasW, atlasH));
    if (!image) {
        delete info;
        return;
    }
    irr::u32* pixelsOut = (irr::u32*)image->getData();
    const irr::u32 pitch = image->getPitch() / 4;
    for (irr::u32 i = 0; i < pitch * atlasH; i++) { pixelsOut[i] = 0x00FFFFFF; }
    std::vector<unsigned char> bitmap;
    for (size_t i = 0; i < boxes.size(); i++) {
        const Box& b = boxes[i];
        if (b.w > 0 && b.h > 0) {
            bitmap.assign((size_t)(b.w * b.h), 0);
            stbtt_MakeGlyphBitmap(info, &bitmap[0], b.w, b.h, b.w, scale, scale, b.index);
            for (int row = 0; row < b.h; row++) {
                irr::u32* line = pixelsOut + (size_t)(b.ay + row) * pitch + b.ax;
                for (int col = 0; col < b.w; col++) {
                    line[col] = ((irr::u32)bitmap[(size_t)(row * b.w + col)] << 24) | 0x00FFFFFF;
                }
            }
        }
        int advance = 0, bearing = 0;
        stbtt_GetGlyphHMetrics(info, b.index, &advance, &bearing);
        Glyph g;
        g.source = irr::core::rect<irr::s32>(b.ax, b.ay, b.ax + b.w, b.ay + b.h);
        g.left = (irr::f32)b.x0;
        g.top = (irr::f32)b.y0;
        g.advance = advance * scale;
        g.index = b.index;
        glyphs[(wchar_t)b.code] = g;
    }

    static int serial = 0;
    const irr::io::path name = (file + "#" + std::to_string((int)pixels) + "#" + std::to_string(serial++)).c_str();
    const bool mipMaps = driver->getTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS);
    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, false);
    texture = driver->addTexture(name, image);
    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, mipMaps);
    image->drop();
    if (!texture) {
        glyphs.clear();
        delete info;
        return;
    }

    face = info;
    ascent = a * scale;
    descent = d * scale;
    caps = stbtt_GetCodepointBox(info, 'H', &bx0, &by0, &bx1, &by1) ? by1 * scale : ascent * 0.7f;
}

HudFont::~HudFont()
{
    if (texture && driver) { driver->removeTexture(texture); }
    delete (stbtt_fontinfo*)face;
}

const HudFont::Glyph* HudFont::glyph(wchar_t c) const
{
    std::map<wchar_t, Glyph>::const_iterator it = glyphs.find(c);
    if (it != glyphs.end()) { return &it->second; }
    //Typographic quotes and dashes the face lacks: the plain ones.
    if (c == 0x2019 || c == 0x2018) { return glyph(L'\''); }
    if (c == 0x2013 || c == 0x2014) { return glyph(L'-'); }
    it = glyphs.find(L'?');
    return it != glyphs.end() ? &it->second : 0;
}

irr::f32 HudFont::kerning(int a, int b) const
{
    if (!face || a <= 0 || b <= 0) { return 0; }
    return stbtt_GetGlyphKernAdvance((const stbtt_fontinfo*)face, a, b) * scale;
}

irr::f32 HudFont::width(const std::wstring& text, irr::f32 tracking) const
{
    if (!texture) {
        return fallback ? (irr::f32)fallback->getDimension(text.c_str()).Width + tracking * text.size() : 0;
    }
    irr::f32 pen = 0;
    int previous = -1;
    for (size_t i = 0; i < text.size(); i++) {
        const Glyph* g = glyph(text[i]);
        if (!g) { continue; }
        pen += kerning(previous, g->index) + g->advance + (i + 1 < text.size() ? tracking : 0);
        previous = g->index;
    }
    return pen;
}

irr::f32 HudFont::draw(const std::wstring& text, irr::f32 x, irr::f32 y, irr::video::SColor col, irr::f32 tracking,
    const irr::core::rect<irr::s32>* clip) const
{
    if (text.empty() || col.getAlpha() == 0) { return 0; }
    if (!texture) {
        if (!fallback) { return 0; }
        const irr::core::dimension2du d = fallback->getDimension(text.c_str());
        const irr::s32 top = (irr::s32)(y - (d.Height - caps) * 0.42f);
        fallback->draw(text.c_str(), irr::core::rect<irr::s32>((irr::s32)x, top, (irr::s32)x + (irr::s32)d.Width + 2, top + (irr::s32)d.Height),
            col, false, false, clip);
        return (irr::f32)d.Width;
    }
    const irr::f32 baseline = y + caps;
    irr::core::array<irr::core::position2di> positions;
    irr::core::array<irr::core::rect<irr::s32> > sources;
    positions.reallocate(text.size());
    sources.reallocate(text.size());
    irr::f32 pen = x;
    int previous = -1;
    for (size_t i = 0; i < text.size(); i++) {
        const Glyph* g = glyph(text[i]);
        if (!g) { continue; }
        pen += kerning(previous, g->index);
        if (g->source.getWidth() > 0) {
            positions.push_back(irr::core::position2di((irr::s32)std::floor(pen + g->left + 0.5f), (irr::s32)std::floor(baseline + g->top + 0.5f)));
            sources.push_back(g->source);
        }
        pen += g->advance + (i + 1 < text.size() ? tracking : 0);
        previous = g->index;
    }
    if (!positions.empty()) { driver->draw2DImageBatch(texture, positions, sources, clip, col, true); }
    return pen - x;
}

void HudFont::drawIn(const std::wstring& text, const irr::core::rect<irr::f32>& box, irr::video::SColor col, Align align,
    irr::f32 tracking, const irr::core::rect<irr::s32>* clip) const
{
    irr::f32 x = box.UpperLeftCorner.X;
    if (align != Left) {
        const irr::f32 w = width(text, tracking);
        x = (align == Centre) ? box.getCenter().X - w * 0.5f : box.LowerRightCorner.X - w;
    }
    draw(text, x, std::floor(box.getCenter().Y - caps * 0.5f + 0.5f), col, tracking, clip);
}

std::vector<std::wstring> HudFont::wrap(const std::wstring& text, irr::f32 maxWidth, irr::f32 tracking) const
{
    std::vector<std::wstring> lines;
    std::wstring line, word;
    for (size_t i = 0; i <= text.size(); i++) {
        const wchar_t ch = (i < text.size()) ? text[i] : L' ';
        if (ch == L' ' || ch == L'\n') {
            const std::wstring candidate = line.empty() ? word : line + L" " + word;
            if (!line.empty() && width(candidate, tracking) > maxWidth) {
                lines.push_back(line);
                line = word;
            }
            else {
                line = candidate;
            }
            word.clear();
            if (ch == L'\n') {
                lines.push_back(line);
                line.clear();
            }
        }
        else {
            word += ch;
        }
    }
    if (!line.empty()) { lines.push_back(line); }
    return lines;
}

std::wstring hudUpper(const std::wstring& text)
{
    std::wstring out(text);
    for (size_t i = 0; i < out.size(); i++) {
        const wchar_t c = out[i];
        if (c >= L'a' && c <= L'z') { out[i] = (wchar_t)(c - 32); }
        else if (c >= 0xE0 && c <= 0xFE && c != 0xF7) { out[i] = (wchar_t)(c - 32); }
        else if (c == 0xFF) { out[i] = (wchar_t)0x178; }
        else if (c == 0x153) { out[i] = (wchar_t)0x152; }
    }
    return out;
}
