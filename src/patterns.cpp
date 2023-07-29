#include "patterns.h"
#include "defines.h"

#include <stdint.h>

namespace Scarf {

static int16_t dist;  // A random number for our noise generator.
uint16_t xscale = 30;  // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint16_t yscale = 30;  // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint8_t maxChanges = 24;  // Value for blending between palettes.

CRGBPalette16 currentPalette(CRGB::White);

void pride(Leds& leds, int32_t timeMs) 
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;
 
  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);
  
  uint16_t ms = timeMs; // millis();//
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5,9);
  uint16_t brightnesstheta16 = sPseudotime;
  
  for( uint16_t i = 0 ; i < kNumLeds; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);
    
    CRGB newcolor = CHSV( hue8, sat8, bri8);
    
    uint16_t pixelnumber = i;
    pixelnumber = (kNumLeds - 1) - pixelnumber;
    
    nblend( leds[pixelnumber], newcolor, 64);
  }
}

void fillnoise8(Leds& leds, int32_t timeMs) {
  // Just ONE loop to fill up the LED array as all of the pixels change.
  for(int i = 0; i < kNumLeds; i++) {
    // Get a value from the noise function. I'm using both x and y axis.
    uint8_t index = inoise8(0, dist + i * yscale) % 255;
    // With that value, look up the 8 bit colour palette value and assign it to the current LED.
    leds[i] = ColorFromPalette(ForestColors_p, index, 255, LINEARBLEND);
  }
  dist += beatsin8(10, 1, 4, timeMs);                        
}

void fillNoise(Leds& leds, int32_t timeMs) {
  fillnoise8(leds, timeMs);
}

int       thishue = 150;                   // Starting hue.

void confetti(Leds& leds, int32_t timeMs) {                                             // random colored speckles that blink in and fade smoothly
    uint8_t  thisfade = 12;                     // How quickly does it fade? Lower = slower fade rate.
    uint8_t   thisinc = 20;                     // Incremental value for rotating hues
    uint8_t   thissat = 255;                   // The saturation, where 255 = brilliant colours.
    uint8_t   thisbri = 200;                   // Brightness of a sequence. Remember, max_bright is the overall limiter.
    int       huediff = 200;                   // Range of random #'s to use for hue

    TBlendType currentBlending = LINEARBLEND_NOWRAP;
    fadeToBlackBy(leds.data(), kNumLeds, thisfade);                    // Low values = slower fade.
    int pos = random16(kNumLeds); 
    CRGBPalette16 palette(LavaColors_p);
    leds[pos] = ColorFromPalette(palette,  thishue + random16(huediff)/4, thisbri, currentBlending);
    thishue = thishue + thisinc;                                   // It increments here.
}


/* =============== FIREWORK ANIMATION =============== */

float easeOutQuart(float t) {
  return 1-(--t)*t*t*t;
} 

float easeOutQuint(float t) {
  return 1+(--t)*t*t*t*t;
}

fract8 timeFrac8(int time, int period) {
  const fract8 frac = (time % period) * 255 / period;
  return frac;
}

void firework(Leds& leds, int32_t timeMs, int periodMs) {
  const auto fireworkFrac = timeFrac8(timeMs, periodMs);
  // Start with easeInVal at 0 and then go to 255 for the full easing.
  const uint8_t fireworkEased = easeOutQuart((float)fireworkFrac / 255) * 255; //ease8InOutCubic(count);

  // Map it to the number of LED's you have.
  const uint8_t fireworkLerpVal = lerp8by8(0, leds.size(), fireworkEased);

  uint8_t index = inoise8(0, dist + fireworkLerpVal * yscale) % 255;
  // With that value, look up the 8 bit colour palette value and assign it to the current LED.
  CRGBPalette16 palette(OceanColors_p);
  leds[fireworkLerpVal] = ColorFromPalette(palette, index, 255, LINEARBLEND);
  leds[fireworkLerpVal].maximizeBrightness();

  fadeToBlackBy(leds.data(), leds.size(), 16);  // 8 bit, 1 = slow fade, 255 = fast fade

//  if (count > 225) {
//    for (int i = NUM_LEDS - 20; i < NUM_LEDS; i++) {
//      index = inoise8(0, dist + lerpVal * yscale) % 255;
//      leds[i] = ColorFromPalette(CRGBPalette16(CHSV(0, 255, 255),
//                                               CHSV(40, 255, random(225, 255)),
//                                               CHSV(80, 255, random(225, 255)),
//                                               CHSV(140, 255, 255)), index, 255, LINEARBLEND);
////      leds[i].fadeToBlackBy(1);
//    }
//  }
  
}

void testpattern(Leds& leds, int32_t timeMs) {
  const int kPeriodMs = 1500;
  const int fireworkCount = timeMs / kPeriodMs;
  auto timeEased = easeOutQuart(fireworkCount / 255.0f) * 255;
  auto lerpVal = lerp8by8(0, leds.size(), timeEased);
  return;
}

// Sliding bar across LEDs
void cylon(Leds& leds, int32_t timeMs, CRGB c, int width, int periodMs){
  const auto timeFrac = timeFrac8(timeMs, periodMs);
  const auto cylonFrac = quadwave8(timeFrac);
  const uint8_t lerpVal = lerp8by8(0, leds.size(), cylonFrac);

  for(int i = 0; i <= leds.size(); ++i) {
    const auto startLed = lerpVal - width / 2;
    const auto stopLed = lerpVal + width / 2;
    const auto isLit = (i >= startLed) && (i <= stopLed);
    if (isLit) {
      leds[i] = c;
    } else {
      leds[i] = CRGB::Black;
    }
  }
}

} // namespace Scarf
