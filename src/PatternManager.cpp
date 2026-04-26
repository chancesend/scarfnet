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

void PatternManager::runCurrentPattern(Leds& leds, TimeMs nodeTimeMs, const BeatInfo& beat,
                                       const NodeInfo& nodeInfo)
{
    PatternContext ctx;
    ctx.timeMs      = nodeTimeMs;
    ctx.palette     = _currentPalette;
    ctx.rnd         = _currentRandomizer;
    ctx.localRnd    = _localRnd;
    ctx.beat        = beat;
    ctx.nodeId      = nodeInfo.nodeId;
    ctx.lastPressId = nodeInfo.lastPressId;
    ctx.flashCount  = nodeInfo.flashCount;
    for (int i = 0; i < nodeInfo.flashCount; i++) ctx.recentFlashes[i] = nodeInfo.flashes[i];
    _currentPattern->second(leds, ctx);
}

void PatternManager::initPatterns()
{
    Scarfnet::log("initPatterns()");
    _patterns = getPatternList();
    _currentPattern = _patterns.begin();
}

void PatternManager::incrementPattern(Rnd randomizer)
{
    auto newPattern = _currentPattern + 1;
    if (newPattern == _patterns.end())
        newPattern = _patterns.begin();
    Scarfnet::log("Scarf::incrementPattern(). Changing pattern to %s", newPattern->first.c_str());
    changePatternFromString(newPattern->first, randomizer);
}

void PatternManager::samePatternDifferentRandomizer(Rnd randomizer)
{
    Scarfnet::log("Scarf::samePatternDifferentRandomizer(). Keeping pattern at %s", _currentPattern->first.c_str());
    changePatternFromString(_currentPattern->first, randomizer);
}

bool PatternManager::changePatternFromString(const std::string &pattern, Rnd randomizer)
{
    auto found = std::find_if(_patterns.begin(), _patterns.end(), [pattern](const NamedPattern &it) -> bool
                                { return (pattern == it.first); });
    if (found != _patterns.end())
    {
        _currentPattern    = found;
        _currentRandomizer = randomizer;
        _localRnd          = (Rnd)random16();  // fresh per-device seed; varies between scarves
        _targetPalette     = getColorPalette(randomizer);
        Scarfnet::log("Changing pattern: %s (randomizer %i localRnd %u)", found->first.c_str(), _currentRandomizer, _localRnd);
        return true;
    }
    else
    {
        Scarfnet::log("Pattern: %s not found — ignoring update", pattern.c_str());
        return false;
    }
}

void PatternManager::blendPalette()
{
    nblendPaletteTowardPalette(_currentPalette, _targetPalette, 24);
}

}
