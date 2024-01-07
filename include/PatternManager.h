#pragma once

#include "patterns.h"
#include "palettes.h"
#include "mesh.h"

namespace Scarfnet
{

class PatternManager
{
public:
    typedef std::shared_ptr<PatternManager> Ptr;
    
    PatternManager();
    ~PatternManager();

    std::string getCurrentPattern();
    
    void runCurrentPattern(Leds& leds, uint32_t nodeTimeMs);

    void incrementPattern(Mesh::TimeMs lastSelfPressMs);
    void samePatternDifferentRandomizer(Mesh::TimeMs lastSelfPressMs);
    void changePatternFromString(const std::string& pattern, Rnd randomizer);

    void currentPatternRun();

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
