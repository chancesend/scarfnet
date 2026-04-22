#pragma once

// Beat position and timing helpers — no hardware, no Arduino, natively testable.
//
// BeatInfo is a snapshot of beat state passed to pattern render functions.
// Construct via TapTempo::beatInfo(now).
//
// Key fields:
//   intervalMs  — beat period in ms (0 = no active tempo)
//   phaseMs     — ms elapsed since last beat onset (0 = on the beat)
//   beatNumber  — total beats since tempo was established (monotonically increasing)
//
// Bar and phrase methods assume 4/4 time by default (beatsPerBar = 4).
//
// ── Envelope methods ─────────────────────────────────────────────────────────
//
// Inactive beat → `start`. All interpolations are linear unless noted.
//
//   sawTime(windowMs, start, end)             — start→end over windowMs, then holds end
//   saw(start, end, duty=1.0, phase=0.0)      — start→end over duty fraction of beat
//   triangle(start, end, duty=1.0, phase=0.0) — start→end→start over duty fraction of beat
//   sin(start, end)                            — smooth bell curve (half-sine), peaks at midbeat
//   square(duty, start, end)                   — end for first duty/255 of beat, start after
//   expDecay(start, end)                       — squared falloff over full beat (punchy, natural)
//
// sawTime: start>end decays, start<end attacks.
// saw: duty=1.0 covers the full beat; duty=0.5 covers the first half.
// phase [0.0..1.0] shifts the shape within the beat for both saw and triangle.
#include "defines.h"
#include <stdint.h>

namespace Scarfnet {

// Compound beat position: bar, beat within bar, subdivision within beat.
// Returned by BeatInfo::position().
struct BarBeat {
    uint16_t bar;         // bars since tempo started, 0-based
    uint8_t  beat;        // beat within bar, 0-based (0 = downbeat)
    uint8_t  subdivision; // subdivision within beat, 0-based (0 = on the subdivision)
    uint8_t  beatFrac8;   // fractional position within the current beat [0..255]
};

struct BeatInfo {
    uint16_t intervalMs = 0;   // beat period in ms; 0 = no active tempo
    uint16_t phaseMs    = 0;   // ms elapsed since the last beat onset
    uint16_t beatNumber = 0;   // total beats since tempo was established

    BeatInfo() = default;
    BeatInfo(uint16_t intervalMs, uint16_t phaseMs, uint16_t beatNumber)
        : intervalMs(intervalMs), phaseMs(phaseMs), beatNumber(beatNumber) {}

    // ── Activation ───────────────────────────────────────────────────────────

    bool isActive() const { return intervalMs > 0; }

    // ── Tempo ────────────────────────────────────────────────────────────────

    // Beats per minute derived from intervalMs. Returns 0 when inactive.
    float tempo() const {
        if (!isActive() || intervalMs == 0) return 0.0f;
        return 60000.0f / intervalMs;
    }

    // ── Beat-level timing ────────────────────────────────────────────────────

    // True within `windowMs` of any beat onset.
    bool isOnBeat(uint16_t windowMs = 60) const {
        return isActive() && phaseMs < windowMs;
    }

    // Fractional position within the current beat [0..255]. 0 = on the beat.
    uint8_t frac8() const {
        if (!isActive() || intervalMs == 0) return 0;
        return (uint8_t)((uint32_t)phaseMs * 255u / intervalMs);
    }

    // ── Envelopes ─────────────────────────────────────────────────

    // Windowed saw: start→end linearly over `windowMs` after onset, then holds end.
    // Returns start when inactive or when called before the onset. Direction is set
    // by caller: start>end decays, start<end attacks.
    // Default (255→0 over 60ms) is the most common percussive-hit use case.
    uint8_t sawTime(uint16_t windowMs = 60, uint8_t start = 255, uint8_t end = 0) const {
        if (!isActive()) return start;
        if (phaseMs >= windowMs) return end;
        return (uint8_t)((int32_t)start
            + (int32_t)((int16_t)end - (int16_t)start) * (int32_t)phaseMs / (int32_t)windowMs);
    }

    // Saw: start→end linearly over `duty` fraction of the period, then holds end.
    // Returns start when inactive. duty=1.0 covers the full period, duty=0.5 the first half.
    // Direction is set by caller: start<end rises, start>end falls.
    // phase shifts the shape within the period [0.0..1.0]; default 0 = aligned to onset.
    uint8_t saw(uint8_t start = 0, uint8_t end = 255, float duty = 1.0f, float phase = 0.0f) const {
        if (!isActive()) return start;
        uint32_t effectiveMs = ((uint32_t)(phaseMs + (uint32_t)(phase * intervalMs))) % intervalMs;
        uint32_t activeMs    = (uint32_t)((float)intervalMs * duty);
        if (activeMs == 0 || effectiveMs >= activeMs) return end;
        return (uint8_t)((int32_t)start
            + (int32_t)((int16_t)end - (int16_t)start) * (int32_t)effectiveMs / (int32_t)activeMs);
    }

    // Triangle: start→end→start over `duty` fraction of the period, then holds start outside.
    // phase shifts the triangle within the period [0.0..1.0]; default 0 = peak at midpoint.
    uint8_t triangle(uint8_t start = 0, uint8_t end = 255, float duty = 1.0f, float phase = 0.0f) const {
        if (!isActive()) return start;
        uint32_t effectiveMs = ((uint32_t)(phaseMs + (uint32_t)(phase * intervalMs))) % intervalMs;
        uint32_t activeMs    = (uint32_t)((float)intervalMs * duty);
        if (activeMs == 0 || effectiveMs >= activeMs) return start;
        uint8_t frac = (uint8_t)((uint32_t)effectiveMs * 255u / activeMs);
        uint8_t tri  = (frac < 128) ? (uint8_t)(frac * 2u) : (uint8_t)((255u - frac) * 2u);
        return lerp8by8(start, end, tri);
    }

    // Smooth bell curve (half-sine): start at onset, peaks at end at midpoint, start at end.
    // Softer attack and release than triangle.
    uint8_t sin(uint8_t start = 0, uint8_t end = 255) const {
        if (!isActive()) return start;
        // sin8(x) = sin(2πx/256)*127+128. Mapping frac/2 into the first half-period
        // gives sin8 output [128..255..128]; recentre and double → [0..254..0].
        uint8_t frac = frac8();
        uint8_t s    = (uint8_t)((uint16_t)(sin8(frac >> 1) - 128u) * 2u);
        return lerp8by8(start, end, s);
    }

    // Square: end for first `duty/255` of the period, start after.
    // duty=128 → 50%, duty=64 → 25%.
    uint8_t square(uint8_t duty = 128, uint8_t start = 0, uint8_t end = 255) const {
        if (!isActive()) return start;
        return (frac8() < duty) ? end : start;
    }

    // Exponential-ish decay over the full period (squared falloff).
    // end at onset, drops quickly then tails off to start — more natural for percussive hits.
    uint8_t expDecay(uint8_t start = 0, uint8_t end = 255) const {
        if (!isActive()) return start;
        uint8_t remain = (uint8_t)(255u - frac8());
        return lerp8by8(start, end, scale8(remain, remain));
    }

    // ── Bar-level ────────────────────────────────────────────────────────────

    // Beat within the current bar, 0-based (0 = downbeat).
    uint8_t beatInBar(uint8_t beatsPerBar = 4) const {
        return isActive() ? (uint8_t)(beatNumber % beatsPerBar) : 0;
    }

    // Bars elapsed since tempo was established, 0-based.
    uint16_t barNumber(uint8_t beatsPerBar = 4) const {
        return isActive() ? (uint16_t)(beatNumber / beatsPerBar) : 0;
    }

    // True within `windowMs` of a bar downbeat (beat 0 of any bar).
    bool isOnBar(uint8_t beatsPerBar = 4, uint16_t windowMs = 60) const {
        return isActive() && (beatNumber % beatsPerBar == 0) && (phaseMs < windowMs);
    }

    // Milliseconds elapsed since the last bar downbeat.
    uint32_t barPhaseMs(uint8_t beatsPerBar = 4) const {
        if (!isActive()) return 0;
        return (uint32_t)(beatNumber % beatsPerBar) * intervalMs + phaseMs;
    }

    // ── Phrase-level ─────────────────────────────────────────────────────────

    // True within `windowMs` of any N-beat phrase boundary (downbeat).
    bool isOnPhrase(uint16_t phraseBeats, uint16_t windowMs = 60) const {
        return isActive() && (beatNumber % phraseBeats == 0) && (phaseMs < windowMs);
    }

    // True on the last beat of every N-beat phrase.
    bool isLastBeatOfPhrase(uint16_t phraseBeats) const {
        return isActive() && (beatNumber % phraseBeats == phraseBeats - 1);
    }

    // Fractional position within the current phrase [0..255]. 0 = phrase downbeat.
    uint8_t phraseFrac8(uint16_t phraseBeats) const {
        if (!isActive() || intervalMs == 0) return 0;
        uint32_t phraseMs = (uint32_t)phraseBeats * intervalMs;
        uint32_t phase    = (uint32_t)(beatNumber % phraseBeats) * intervalMs + phaseMs;
        return (uint8_t)(phase * 255u / phraseMs);
    }

    // Fractional position within the current phrase [0..65535]. 0 = phrase downbeat.
    // Higher precision than phraseFrac8 — useful for long phrases where 8-bit resolution
    // is too coarse. Uses 64-bit arithmetic internally to avoid overflow.
    uint16_t phraseFrac16(uint16_t phraseBeats) const {
        if (!isActive() || intervalMs == 0) return 0;
        uint32_t phraseMs = (uint32_t)phraseBeats * intervalMs;
        uint32_t phase    = (uint32_t)(beatNumber % phraseBeats) * intervalMs + phaseMs;
        return (uint16_t)(((uint64_t)phase * 65535u) / phraseMs);
    }

    // Returns a BeatInfo scaled to phrase duration, so all beat-level envelope methods
    // (beatSaw, beatTriangle, beatSin, beatSquare, beatExpDecay, beatSawTime) operate
    // over the full phrase period instead of a single beat.
    //
    // Usage:
    //   beat.phraseView(16).beatSaw(0, 255)       — rising saw over 16 beats
    //   beat.phraseView(16).beatTriangle(0, 255)  — triangle peaking at phrase midpoint
    //   beat.phraseView(32).beatSin(40, 255)      — smooth swell over 32 beats
    //
    // The virtual intervalMs is normalised to 10000 so it fits in uint16_t regardless
    // of phrase length. beatNumber in the returned view is the phrase count.
    BeatInfo phraseView(uint16_t phraseBeats) const {
        if (!isActive() || intervalMs == 0) return BeatInfo{};
        static constexpr uint16_t kVirtMs = 10000;
        uint32_t phraseMs    = (uint32_t)phraseBeats * intervalMs;
        uint32_t phaseInPhrase = (uint32_t)(beatNumber % phraseBeats) * intervalMs + phaseMs;
        uint16_t scaledPhase = (uint16_t)((uint32_t)phaseInPhrase * kVirtMs / phraseMs);
        return BeatInfo{kVirtMs, scaledPhase, (uint16_t)(beatNumber / phraseBeats)};
    }

    // ── Compound position ────────────────────────────────────────────────────

    // Full breakdown of the current position.
    // subdivisionsPerBeat=2 gives 8th notes, 4 gives 16th notes.
    BarBeat position(uint8_t beatsPerBar = 4, uint8_t subdivisionsPerBeat = 2) const {
        if (!isActive() || intervalMs == 0) return BarBeat{0, 0, 0, 0};
        uint32_t subDivMs = intervalMs / subdivisionsPerBeat;
        return BarBeat{
            (uint16_t)(beatNumber / beatsPerBar),
            (uint8_t)(beatNumber % beatsPerBar),
            (subDivMs > 0) ? (uint8_t)(phaseMs / subDivMs) : (uint8_t)0,
            (uint8_t)((uint32_t)phaseMs * 255u / intervalMs),
        };
    }
};

} // namespace Scarfnet
