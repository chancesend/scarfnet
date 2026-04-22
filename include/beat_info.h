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

    // ── Beat-level ───────────────────────────────────────────────────────────

    // True within `windowMs` of any beat onset.
    bool isOnBeat(uint16_t windowMs = 60) const {
        return isActive() && phaseMs < windowMs;
    }

    // Fractional position within the current beat [0..255]. 0 = on the beat.
    uint8_t beatFrac8() const {
        if (!isActive() || intervalMs == 0) return 0;
        return (uint8_t)((uint32_t)phaseMs * 255u / intervalMs);
    }

    // Brightness that peaks at 255 on the beat and fades to 0 at `windowMs`.
    uint8_t flashBrightness(uint16_t windowMs = 60) const {
        if (!isOnBeat(windowMs)) return 0;
        return (uint8_t)(255u - (uint32_t)phaseMs * 255u / windowMs);
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

    // Fractional position within the current bar [0..255]. 0 = bar downbeat.
    uint8_t barFrac8(uint8_t beatsPerBar = 4) const {
        if (!isActive() || intervalMs == 0) return 0;
        uint32_t barMs  = (uint32_t)beatsPerBar * intervalMs;
        uint32_t phase  = barPhaseMs(beatsPerBar);
        return (uint8_t)(phase * 255u / barMs);
    }

    // ── Phrase-level ─────────────────────────────────────────────────────────

    // True within `windowMs` of any N-beat phrase boundary.
    bool isOnPhrase(uint16_t phraseBeats, uint16_t windowMs = 60) const {
        return isActive() && (beatNumber % phraseBeats == 0) && (phaseMs < windowMs);
    }

    // Brightness that peaks at 255 on a phrase boundary and fades to 0 at `windowMs`.
    uint8_t phraseFlashBrightness(uint16_t phraseBeats, uint16_t windowMs = 60) const {
        if (!isOnPhrase(phraseBeats, windowMs)) return 0;
        return (uint8_t)(255u - (uint32_t)phaseMs * 255u / windowMs);
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
