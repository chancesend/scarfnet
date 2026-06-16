#pragma once

#include "mesh_types.h" // TimeMs
#include "patterns.h"
#include "palettes.h"
#include <cstdint>
#include <memory>

namespace Scarfnet
{

struct NodeInfo {
    NodeId nodeId;
    NodeId lastPressId;
    const NodeFlash* flashes;
    int flashCount;
};

// Owns the LED pattern list and tracks which pattern is currently active.
// All pattern changes are driven through this class so state stays consistent.
class PatternManager
{
public:
    typedef std::shared_ptr<PatternManager> Ptr;

    PatternManager();
    ~PatternManager();

    // Returns the name of the currently active pattern.
    std::string getCurrentPattern();

    // Renders the current pattern into leds using the given mesh timestamp and beat state.
    void runCurrentPattern(Leds& leds, TimeMs nodeTimeMs, const BeatInfo& beat,
                            const NodeInfo& nodeInfo);

    // Switches to a random pattern (excluding current) and updates the randomizer seed.
    void incrementPattern(Rnd randomizer);
    // Keeps the current pattern but re-seeds the randomizer for a different look.
    void samePatternDifferentRandomizer(Rnd randomizer);
    // Switches to the named pattern with the given randomizer (used when syncing from a remote node).
    // Returns true if the pattern was found and applied, false if the name was unrecognized.
    bool changePatternFromString(const std::string& pattern, Rnd randomizer);

    // Interpolates the current palette toward the target over kPaletteTransitionMs.
    void blendPalette(TimeMs nodeTimeMs);

private:
    void initPatterns();

    PatternList _patterns;
    PatternList::iterator _currentPattern;
    CRGBPalette16 _currentPalette {CRGB::Black};
    CRGBPalette16 _targetPalette {RainbowColors_p};
    Rnd _currentRandomizer {0};
    Rnd _localRnd          {0};  // re-randomized locally on each pattern/seed change

    // Pattern cross-fade: previous pattern state retained until transition completes.
    PatternList::iterator _previousPattern;
    Rnd      _previousRandomizer {0};
    Rnd      _previousLocalRnd   {0};
    TimeMs   _transitionStartMs  {0};   // 0 = no transition in progress
    bool     _transitionPending  {false};

    // Palette cross-fade: lerp from _prevPalette → _targetPalette over kPaletteTransitionMs.
    CRGBPalette16 _prevPalette   {CRGB::Black};
    TimeMs   _paletteStartMs     {0};   // 0 = no palette transition in progress
    bool     _palettePending     {true};  // true on boot so palette fades in immediately

    // Recency tracking for biased pattern selection.
    // _lastUsed[i] = _selectCount value when pattern i was last chosen.
    // Weight = (_selectCount - _lastUsed[i] + 1), so older patterns are favoured.
    std::vector<uint32_t> _lastUsed;
    uint32_t              _selectCount {0};


};

};
