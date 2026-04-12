#include "PatternManager.h"
#include "log.h"

#include <string>


namespace Scarfnet {

PatternManager::PatternManager()
{
    initPatterns();
}

PatternManager::~PatternManager()
{
    
}

std::string PatternManager::getCurrentPattern()
{
    return _currentPattern->first;
}

void PatternManager::runCurrentPattern(Leds& leds, uint32_t nodeTimeMs)
{
    _currentPattern->second(
        leds,
        nodeTimeMs,
        _currentPalette,
        _currentRandomizer);
}

void PatternManager::initPatterns()
{
    Scarfnet::log("initPatterns()\n");
    _patterns = getPatternList();
    _currentPattern = _patterns.begin();
}

void PatternManager::incrementPattern(Mesh::TimeMs lastSelfPressMs)
{
    auto newPattern = _currentPattern + 1;
    if (newPattern == _patterns.end())
    {
        newPattern = _patterns.begin();
    }
    const auto newName = newPattern->first.c_str();
    Scarfnet::log("Scarf::incrementPattern(). Changing pattern to %s\n", newName);
    const auto randomizer = lastSelfPressMs;
    changePatternFromString(newPattern->first, randomizer);
}

void PatternManager::samePatternDifferentRandomizer(Mesh::TimeMs lastSelfPressMs)
{
    auto newPattern = _currentPattern;
    const auto newName = newPattern->first.c_str();
    Scarfnet::log("Scarf::samePatternDifferentRandomizer(). Keeping pattern at %s\n", newName);
    const auto randomizer = lastSelfPressMs;
    changePatternFromString(newPattern->first, randomizer);
}

void PatternManager::changePatternFromString(const std::string &pattern, Rnd randomizer)
{
    auto found = std::find_if(_patterns.begin(), _patterns.end(), [pattern](const NamedPattern &it) -> bool
                                { return (pattern == it.first); });
    if (found != _patterns.end())
    {
        _currentPattern = found;
        _currentRandomizer = randomizer;
        _targetPalette = getColorPalette(randomizer);
        Scarfnet::log("Changing pattern: %s (randomizer %i)\n", found->first.c_str(), _currentRandomizer);
    }
    else
    {
        Scarfnet::log("Pattern: %s not found!\n", found->first.c_str());
    }
}

void PatternManager::blendPalette()
{
    nblendPaletteTowardPalette(_currentPalette, _targetPalette, 24);
}

}
