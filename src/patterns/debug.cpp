#include "patterns.h"

namespace Scarfnet {

// Maps a NodeId deterministically to a hue via Knuth multiplicative hash.
static CRGB nodeColor(NodeId id) {
    uint8_t hue = (uint8_t)((id * 2654435761u) >> 24);
    return CHSV(hue, 220, 255);
}

// Debug pattern — makes each scarf's role in the mesh visually obvious:
//
//   Base glow   : dim version of this scarf's own color (derived from its node ID)
//   Flash pulse : each received heartbeat briefly floods the strip with the
//                 sender's color, fading to black over kFlashFadeMs
//   Sparkle     : if this scarf was the last to change the pattern, occasional
//                 white pixels confirm it is "in control"
void debug(Leds& leds, const PatternContext& ctx)
{
    constexpr uint8_t kFlashBrightness8 = 100;
    constexpr uint32_t kFlashFadeMs = 100;
    constexpr uint8_t kDimScale8 = 1; // out of 256; lower = dimmer base glow

    // Dim base: own persistent color so the scarf is always identifiable.
    CRGB base = nodeColor(ctx.nodeId);
    fill_solid(leds.data(), leds.size(), CRGB(base).nscale8(kDimScale8));
    // Additive flash overlay for each recently-heard node.
    for (int i = 0; i < ctx.flashCount; i++) {
        uint32_t elapsed = (ctx.timeMs >= ctx.recentFlashes[i].when)
                         ? ctx.timeMs - ctx.recentFlashes[i].when : 0;
        if (elapsed >= kFlashFadeMs) continue;
        uint8_t alpha = (uint8_t)(kFlashBrightness8 - kFlashBrightness8 * elapsed / kFlashFadeMs);
        CRGB flash = CRGB(nodeColor(ctx.recentFlashes[i].id)).nscale8(alpha);
        for (auto& led : leds)
            led += flash;
    }

    // Sparkle white if this scarf last changed the pattern.
    constexpr uint8_t kSparkleRate = 25;  // out of 255; higher = more frequent white sparkles
    if (ctx.nodeId != 0 && ctx.lastPressId == ctx.nodeId) {
        if (random8() < kSparkleRate)
            leds[random16() % leds.size()] = CRGB(CRGB::White).nscale8(kFlashBrightness8);
    }
}

} // namespace Scarfnet
