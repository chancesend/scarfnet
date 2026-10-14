#include "PatternManager.h"
#include "config.h"
#include "log.h"

#include <string>


namespace Scarfnet {

PatternManager::PatternManager()
{
    initPatterns();
    _previousPattern = _patterns.begin();
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
    ctx.beat        = beat;
    ctx.nodeId      = nodeInfo.nodeId;
    ctx.lastPressId = nodeInfo.lastPressId;
    ctx.flashCount  = nodeInfo.flashCount;
    for (int i = 0; i < nodeInfo.flashCount; i++) ctx.recentFlashes[i] = nodeInfo.flashes[i];

    // Cross-fade: blend outgoing pattern into incoming over kPatternTransitionMs.
    uint8_t blendAmt;
    if (_patternFade.step(nodeTimeMs, blendAmt)) {
        // Render outgoing pattern into a scratch buffer.
        Leds prevLeds(leds.size(), CRGB::Black);
        PatternContext prevCtx  = ctx;
        prevCtx.rnd      = _previousRandomizer;
        prevCtx.localRnd = _previousLocalRnd;
        _previousPattern->second(prevLeds, prevCtx);

        // Render incoming pattern into leds.
        ctx.rnd      = _currentRandomizer;
        ctx.localRnd = _localRnd;
        _currentPattern->second(leds, ctx);

        // Blend: 0 = all outgoing, 255 = all incoming.
        for (size_t i = 0; i < leds.size(); ++i)
            leds[i] = blend(prevLeds[i], leds[i], blendAmt);
        return;
    }

    ctx.rnd      = _currentRandomizer;
    ctx.localRnd = _localRnd;
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
        _previousPattern    = _currentPattern;
        _previousRandomizer = _currentRandomizer;
        _previousLocalRnd   = _localRnd;
        _patternFade.begin();

        _prevPalette    = _currentPalette;
        _paletteFade.begin();

        _currentPattern    = found;
        _currentRandomizer = randomizer;
        _localRnd          = (Rnd)random16();  // fresh per-device seed; varies between scarves
        _targetPalette     = getColorPalette(randomizer);
        Scarfnet::log("Changing pattern: %s (randomizer %i localRnd %u palette=%s)",
                      found->first.c_str(), _currentRandomizer, _localRnd,
                      getPaletteName(_currentRandomizer));
        return true;
    }
    else
    {
        Scarfnet::log("Pattern: %s not found — ignoring update", pattern.c_str());
        return false;
    }
}

void PatternManager::blendPalette(TimeMs nodeTimeMs)
{
    uint8_t amt;
    if (!_paletteFade.step(nodeTimeMs, amt)) {
        _currentPalette = _targetPalette;  // finished (or idle): settle on target
        return;
    }

    // Direct time-based lerp across all 48 palette bytes.
    uint8_t* cur  = (uint8_t*)&_currentPalette;
    uint8_t* prev = (uint8_t*)&_prevPalette;
    uint8_t* tgt  = (uint8_t*)&_targetPalette;
    for (int i = 0; i < 48; ++i)
        cur[i] = lerp8by8(prev[i], tgt[i], amt);
}

}
