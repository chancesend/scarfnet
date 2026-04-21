#pragma once
// Native-compatible FastLED stub for the scarfnet simulator.
// Provides CRGB, CRGBPalette16, and all math/noise/palette functions used by patterns.
// Include this instead of <FastLED.h> when building without the Arduino toolchain.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <chrono>
#include <vector>

using std::min;
using std::max;

// ─── Basic types ──────────────────────────────────────────────────────────────
typedef uint8_t  fract8;
typedef uint16_t fract16;
enum TBlendType { NOBLEND = 0, LINEARBLEND = 1, LINEARBLEND_NOWRAP = 2 };

// ─── Math ─────────────────────────────────────────────────────────────────────
inline uint8_t lerp8by8(uint8_t a, uint8_t b, fract8 frac) {
    if (b > a) return a + (uint8_t)(((uint16_t)(b - a) * frac) >> 8);
    return       a - (uint8_t)(((uint16_t)(a - b) * frac) >> 8);
}
inline uint16_t lerp16by8(uint16_t a, uint16_t b, fract8 frac) {
    if (b > a) return a + (uint16_t)(((uint32_t)(b - a) * frac) >> 8);
    return       a - (uint16_t)(((uint32_t)(a - b) * frac) >> 8);
}
inline uint8_t scale8(uint8_t i, uint8_t sc) {
    return (uint8_t)(((uint16_t)i * (1 + (uint16_t)sc)) >> 8);
}
inline uint8_t qadd8(uint8_t i, uint8_t j) {
    uint16_t t = (uint16_t)i + j; return t > 255 ? 255 : (uint8_t)t;
}
inline uint8_t qsub8(uint8_t i, uint8_t j) { return i > j ? i - j : 0; }
inline uint8_t qmul8(uint8_t i, uint8_t j) {
    uint16_t t = (uint16_t)i * j; return t > 255 ? 255 : (uint8_t)t;
}
inline uint8_t ease8InOutCubic(uint8_t i) {
    uint8_t ii  = scale8(i, i);
    uint8_t iii = scale8(ii, i);
    return (uint8_t)((uint16_t)(3 * (uint16_t)ii) - (uint16_t)(2 * (uint16_t)iii));
}

// ─── Trig ─────────────────────────────────────────────────────────────────────
inline uint8_t sin8(uint8_t theta) {
    return (uint8_t)(128.5 + 127.5 * sin(theta * (2.0 * M_PI / 256.0)));
}
inline uint8_t cos8(uint8_t theta) {
    return (uint8_t)(128.5 + 127.5 * cos(theta * (2.0 * M_PI / 256.0)));
}
inline int16_t sin16(uint16_t theta) {
    return (int16_t)(32767.5 * sin(theta * (2.0 * M_PI / 65536.0)));
}
inline uint8_t quadwave8(uint8_t in) {
    // Smooth bounce: rises 0→255 over [0,128], falls 255→0 over [128,255]
    return ease8InOutCubic(in < 128 ? (uint8_t)(in * 2) : (uint8_t)((255 - in) * 2));
}

// beatsin88 / beatsin8: oscillate at the given BPM using wall-clock time.
// FastLED uses GET_MILLIS() internally; we use std::chrono for a faithful stub.
inline uint64_t _simWallMs() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
inline uint16_t beatsin88(uint16_t bpm88, uint16_t low = 0, uint16_t high = 65535,
                           uint16_t timebase = 0, uint8_t phase_offset = 0) {
    uint32_t period = (uint32_t)(60000ULL * 256 / bpm88);
    double t = (double)((_simWallMs() - timebase) % period) / period;
    double s = 0.5 + 0.5 * sin(t * 2.0 * M_PI + phase_offset * (2.0 * M_PI / 256.0));
    return (uint16_t)(low + (uint32_t)((high - low) * s));
}
inline uint8_t beatsin8(uint8_t bpm, uint8_t low = 0, uint8_t high = 255,
                         uint32_t timebase = 0, uint8_t phase_offset = 0) {
    uint32_t period = (uint32_t)(60000u / bpm);
    double t = (double)((_simWallMs() - timebase) % period) / period;
    double s = 0.5 + 0.5 * sin(t * 2.0 * M_PI + phase_offset * (2.0 * M_PI / 256.0));
    return (uint8_t)(low + (uint32_t)((high - low) * s));
}

// ─── Random ───────────────────────────────────────────────────────────────────
inline uint8_t  random8()             { return (uint8_t)(rand() & 0xFF); }
inline uint8_t  random8(uint8_t lim)  { return lim ? (uint8_t)(rand() % lim) : 0; }
inline uint16_t random16()            { return (uint16_t)(rand() & 0xFFFF); }
inline uint16_t random16(uint16_t lim){ return lim ? (uint16_t)(rand() % lim) : 0; }
inline void     random16_set_seed(uint16_t s) { srand(s); }

// ─── Noise ────────────────────────────────────────────────────────────────────
// Value noise with smooth interpolation — visually similar to FastLED's inoise8.
// Clustered around 128 like Perlin noise; thresholds in patterns work as expected.
namespace {
    inline uint8_t _nhash(uint8_t x, uint8_t y) {
        uint16_t v = (uint16_t)x * 2053u + (uint16_t)y * 26729u + 33613u;
        v ^= v >> 7; v ^= v << 3; return (uint8_t)(v & 0xFF);
    }
}
inline uint8_t inoise8(uint16_t x, uint16_t y = 0) {
    uint8_t ix = (uint8_t)(x >> 8), fx = (uint8_t)(x & 0xFF);
    uint8_t iy = (uint8_t)(y >> 8), fy = (uint8_t)(y & 0xFF);
    uint8_t sx = ease8InOutCubic(fx), sy = ease8InOutCubic(fy);
    uint8_t v00 = _nhash(ix,   iy),   v10 = _nhash(ix+1, iy);
    uint8_t v01 = _nhash(ix,   iy+1), v11 = _nhash(ix+1, iy+1);
    uint8_t v0  = lerp8by8(v00, v10, sx);
    uint8_t v1  = lerp8by8(v01, v11, sx);
    uint8_t raw = lerp8by8(v0, v1, sy);
    // Remap to [76, 180] to approximate FastLED's inoise8 clustering around 128
    return (uint8_t)(76u + (uint16_t)raw * 104u / 255u);
}

// ─── CRGB ─────────────────────────────────────────────────────────────────────
struct CRGB {
    union { struct { uint8_t r, g, b; }; uint8_t raw[3]; };

    CRGB() : r(0), g(0), b(0) {}
    CRGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

    CRGB& operator=(const CRGB&) = default;
    bool operator==(const CRGB& o) const { return r==o.r && g==o.g && b==o.b; }

    uint8_t getLuma() const {
        return (uint8_t)(((uint16_t)r*54 + (uint16_t)g*182 + (uint16_t)b*19) >> 8);
    }
    void maximizeBrightness(uint8_t limit = 255) {
        uint8_t m = max(r, max(g, b));
        if (m == 0) return;
        uint16_t s = (uint16_t)limit * 255u / m;
        r = (uint8_t)min((uint32_t)255, (uint32_t)r * s / 255);
        g = (uint8_t)min((uint32_t)255, (uint32_t)g * s / 255);
        b = (uint8_t)min((uint32_t)255, (uint32_t)b * s / 255);
    }

    static const CRGB Black, White, Red, Green, Blue;
};

inline const CRGB CRGB::Black {  0,   0,   0};
inline const CRGB CRGB::White {255, 255, 255};
inline const CRGB CRGB::Red   {255,   0,   0};
inline const CRGB CRGB::Green {  0, 255,   0};
inline const CRGB CRGB::Blue  {  0,   0, 255};

// ─── Palette ──────────────────────────────────────────────────────────────────
struct CRGBPalette16 {
    CRGB entries[16];
    CRGBPalette16() {}
    explicit CRGBPalette16(const CRGB& c) { for (auto& e : entries) e = c; }
    CRGBPalette16(CRGB c0, CRGB c1, CRGB c2, CRGB c3) {
        for (int i = 0; i < 4; ++i) {
            entries[i]    = c0; entries[i+4]  = c1;
            entries[i+8]  = c2; entries[i+12] = c3;
        }
    }
    CRGB&       operator[](uint8_t i)       { return entries[i & 15]; }
    const CRGB& operator[](uint8_t i) const { return entries[i & 15]; }
};

// HSV → RGB helper (FastLED-compatible)
inline CRGB _hsv2rgb(uint8_t h, uint8_t s, uint8_t v) {
    if (s == 0) return CRGB(v, v, v);
    uint8_t r = h / 43;
    uint8_t f = (uint8_t)((h % 43) * 6);
    uint8_t p = (uint8_t)((uint16_t)v * (255 - s) / 255);
    uint8_t q = (uint8_t)((uint16_t)v * (255 - (uint16_t)s * f / 255) / 255);
    uint8_t t = (uint8_t)((uint16_t)v * (255 - (uint16_t)s * (255 - f) / 255) / 255);
    switch (r) {
        case 0: return CRGB(v, t, p); case 1: return CRGB(q, v, p);
        case 2: return CRGB(p, v, t); case 3: return CRGB(p, q, v);
        case 4: return CRGB(t, p, v); default: return CRGB(v, p, q);
    }
}

inline CRGBPalette16 _makeRainbow() {
    CRGBPalette16 p;
    for (int i = 0; i < 16; ++i) p.entries[i] = _hsv2rgb((uint8_t)(i * 16), 255, 220);
    return p;
}
inline CRGBPalette16 _makeForest() {
    CRGBPalette16 p;
    const uint8_t hues[16] = {96,96,100,104,108,96,112,88,96,100,96,120,90,96,104,96};
    const uint8_t sats[16] = {200,180,220,200,160,220,180,200,220,200,200,180,200,220,200,180};
    const uint8_t vals[16] = {120,100,160,140,120,160,100,140,120,160,140,120,160,100,140,120};
    for (int i = 0; i < 16; ++i) p.entries[i] = _hsv2rgb(hues[i], sats[i], vals[i]);
    return p;
}

inline CRGBPalette16 _makeCloud() {
    CRGBPalette16 p;
    const uint8_t hues[16] = {160,160,160,160,160,160,160,160,
                               160,160,160,160,160,160,160,160};
    const uint8_t sats[16] = {200,180,160,140,120,100, 80, 60,
                                40, 30, 20, 10,  5,  0,  0,  0};
    const uint8_t vals[16] = {128,140,154,168,182,196,210,220,
                               230,235,240,245,248,250,252,255};
    for (int i = 0; i < 16; ++i) p.entries[i] = _hsv2rgb(hues[i], sats[i], vals[i]);
    return p;
}
inline CRGBPalette16 _makeLava() {
    CRGBPalette16 p;
    const uint8_t hues[16] = {  0,  0,  4,  8, 12, 16, 20, 24,
                                28, 32, 36, 40, 44, 48, 56, 64};
    const uint8_t sats[16] = {255,255,255,255,255,255,255,255,
                               255,255,240,220,200,180,160,140};
    const uint8_t vals[16] = {  0, 20, 50, 90,130,160,185,205,
                               220,235,245,250,252,254,255,255};
    for (int i = 0; i < 16; ++i) p.entries[i] = _hsv2rgb(hues[i], sats[i], vals[i]);
    return p;
}
inline CRGBPalette16 _makeOcean() {
    CRGBPalette16 p;
    const uint8_t hues[16] = {140,145,150,155,160,163,166,169,
                               172,175,178,180,182,184,186,188};
    const uint8_t sats[16] = {255,240,225,210,200,190,180,170,
                               165,160,155,150,145,140,135,130};
    const uint8_t vals[16] = { 20, 40, 60, 80,100,120,140,160,
                               175,188,198,208,218,228,238,248};
    for (int i = 0; i < 16; ++i) p.entries[i] = _hsv2rgb(hues[i], sats[i], vals[i]);
    return p;
}
inline CRGBPalette16 _makeParty() {
    CRGBPalette16 p;
    for (int i = 0; i < 16; ++i) p.entries[i] = _hsv2rgb((uint8_t)(i * 16), 240, 230);
    return p;
}
inline CRGBPalette16 _makeHeat() {
    CRGBPalette16 p;
    const uint8_t hues[16] = {  0,  0,  0,  0,  4,  8, 12, 16,
                                20, 24, 28, 32, 36, 40, 48, 60};
    const uint8_t sats[16] = {255,255,255,255,255,255,255,255,
                               255,255,240,220,200,180,160,128};
    const uint8_t vals[16] = {  0, 32, 64, 96,128,150,170,188,
                               205,218,228,238,245,250,254,255};
    for (int i = 0; i < 16; ++i) p.entries[i] = _hsv2rgb(hues[i], sats[i], vals[i]);
    return p;
}

// Named palette globals (inline C++17 — one definition across TUs)
inline const CRGBPalette16 RainbowColors_p        = _makeRainbow();
inline const CRGBPalette16 RainbowStripeColors_p  = _makeRainbow();
inline const CRGBPalette16 ForestColors_p         = _makeForest();
inline const CRGBPalette16 CloudColors_p          = _makeCloud();
inline const CRGBPalette16 LavaColors_p           = _makeLava();
inline const CRGBPalette16 OceanColors_p          = _makeOcean();
inline const CRGBPalette16 PartyColors_p          = _makeParty();
inline const CRGBPalette16 HeatColors_p           = _makeHeat();

inline CRGB ColorFromPalette(const CRGBPalette16& pal, uint8_t index,
                              uint8_t brightness = 255,
                              TBlendType blendType = LINEARBLEND) {
    uint8_t lo = index >> 4;
    uint8_t hi = (lo + 1) & 15;
    uint8_t f  = (uint8_t)((index & 0x0F) << 4);
    CRGB c;
    if (blendType == NOBLEND) {
        c = pal.entries[lo];
    } else {
        c = CRGB(lerp8by8(pal.entries[lo].r, pal.entries[hi].r, f),
                 lerp8by8(pal.entries[lo].g, pal.entries[hi].g, f),
                 lerp8by8(pal.entries[lo].b, pal.entries[hi].b, f));
    }
    if (brightness != 255) {
        c.r = scale8(c.r, brightness);
        c.g = scale8(c.g, brightness);
        c.b = scale8(c.b, brightness);
    }
    return c;
}

// ─── LED operations ───────────────────────────────────────────────────────────
inline CRGB blend(const CRGB& a, const CRGB& b, fract8 amt) {
    return CRGB(lerp8by8(a.r, b.r, amt),
                lerp8by8(a.g, b.g, amt),
                lerp8by8(a.b, b.b, amt));
}
inline void nblend(CRGB& existing, const CRGB& overlay, fract8 amt) {
    existing = blend(existing, overlay, amt);
}
inline void fadeToBlackBy(CRGB* leds, uint16_t num, uint8_t fadeBy) {
    uint8_t keep = 255 - fadeBy;
    for (uint16_t i = 0; i < num; ++i) {
        leds[i].r = scale8(leds[i].r, keep);
        leds[i].g = scale8(leds[i].g, keep);
        leds[i].b = scale8(leds[i].b, keep);
    }
}
inline void blur1d(CRGB* leds, uint16_t num, fract8 blur) {
    uint8_t keep = 255 - blur, seep = blur >> 1;
    CRGB carry{0,0,0};
    for (uint16_t i = 0; i < num; ++i) {
        CRGB cur  = leds[i];
        CRGB part = CRGB(scale8(cur.r, seep), scale8(cur.g, seep), scale8(cur.b, seep));
        cur.r = qadd8(scale8(cur.r, keep), carry.r);
        cur.g = qadd8(scale8(cur.g, keep), carry.g);
        cur.b = qadd8(scale8(cur.b, keep), carry.b);
        leds[i] = cur; carry = part;
    }
}

// ─── Palette blending ─────────────────────────────────────────────────────────
inline void nblendPaletteTowardPalette(CRGBPalette16& cur, const CRGBPalette16& tgt,
                                        uint8_t maxChanges) {
    for (int i = 0; i < 16; ++i) {
        auto step = [maxChanges](uint8_t& c, uint8_t t) {
            if (c < t) c = (uint8_t)min((int)c + maxChanges, (int)t);
            else if (c > t) c = (uint8_t)max((int)c - maxChanges, (int)t);
        };
        step(cur.entries[i].r, tgt.entries[i].r);
        step(cur.entries[i].g, tgt.entries[i].g);
        step(cur.entries[i].b, tgt.entries[i].b);
    }
}
