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
    _lastUsed.assign(_patterns.size(), 0);
    _selectCount = 0;
}

void PatternManager::incrementPattern(Rnd randomizer)
{
    // Weighted random selection biased toward patterns not seen recently.
    // Weight for each candidate = (_selectCount - _lastUsed[i] + 1), so a pattern
    // unseen for N presses has (N+1)x the weight of one just shown. All patterns
    // start equal (weight 1) and the current pattern is always excluded.
    size_t n   = _patterns.size();
    size_t cur = (size_t)(_currentPattern - _patterns.begin());

    uint32_t totalWeight = 0;
    for (size_t i = 0; i < n; ++i) {
        if (i == cur) continue;
        totalWeight += (_selectCount - _lastUsed[i] + 1);
    }

    uint32_t pick  = (uint32_t)randomizer % totalWeight;
    uint32_t accum = 0;
    size_t   chosen = (cur + 1) % n;  // fallback: next pattern
    for (size_t i = 0; i < n; ++i) {
        if (i == cur) continue;
        accum += (_selectCount - _lastUsed[i] + 1);
        if (pick < accum) { chosen = i; break; }
    }

    _selectCount++;
    _lastUsed[chosen] = _selectCount;

    auto newPattern = _patterns.begin() + (ptrdiff_t)chosen;
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
