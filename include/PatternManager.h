#pragma once

#include "patterns.h"
#include "palettes.h"
#include <cstdint>
#include <memory>

namespace Scarfnet
{

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

    // Renders the current pattern into leds using the given mesh timestamp.
    void runCurrentPattern(Leds& leds, uint32_t nodeTimeMs);

    // Advances to the next pattern and updates the randomizer seed from the press timestamp.
    void incrementPattern(uint32_t lastSelfPressMs);
    // Keeps the current pattern but re-seeds the randomizer for a different look.
    void samePatternDifferentRandomizer(uint32_t lastSelfPressMs);
    // Switches to the named pattern with the given randomizer (used when syncing from a remote node).
    void changePatternFromString(const std::string& pattern, Rnd randomizer);

    // Steps the current palette one blend tick toward the target palette.
    void blendPalette();

private:
    void initPatterns();

    PatternList _patterns;
    PatternList::iterator _currentPattern;
    CRGBPalette16 _currentPalette {CRGB::Black};
    CRGBPalette16 _targetPalette {RainbowColors_p};
    Rnd _currentRandomizer {0};


};

};
