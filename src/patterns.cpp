#include "patterns.h"
#include "defines.h"
#include "palettes.h"

#include <FastLED.h>

#include <stdint.h>

namespace Scarf {

inline uint8_t interpUint8(uint8_t val, uint8_t low, uint8_t high)
{
  auto range = high - low;
  return val % (range) + low;
}

void getPatternList(PatternList& patterns)
{
    patterns.push_back({"pride", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd randomizer) {
        pride(leds, timeMs, palette);
    }});
    patterns.push_back({"confetti", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd randomizer) {
        uint8_t  thisFade = interpUint8((randomizer % 25), 1, 25);                     // How quickly does it fade? Lower = slower fade rate.
        uint8_t popChance = interpUint8((randomizer % 25) * 4, 0, 100);
        confetti(leds, timeMs, palette, thisFade, popChance);
    }});
    patterns.push_back({"firework", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd randomizer) {
        const uint8_t periodInterp = interpUint8((randomizer % 25) * 3, 10, 85);
        const int periodMs = periodInterp * 100;
        firework(leds, timeMs, periodMs, palette);
    }});
    patterns.push_back({"colorwaves", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd randomizer) {
        colorwaves( leds, timeMs, palette);
    }});
    patterns.push_back({"cylon", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd randomizer) {
        int width = interpUint8((randomizer % 25) * 10, 1, 10);
        const uint8_t periodInterp = interpUint8((randomizer % 25) * 2, 10, 60);
        int periodMs = periodInterp * 100;
  //      CHSV hsv(randomizer, 255, 200);
  //      CRGB color(hsv);
        fract8 blurAmount = interpUint8((randomizer % 25) * 4, 0, 100);
        auto paletteChangeDivisor = randomizer % 10 + 5;
        auto color = ColorFromPalette(palette, (timeMs >> paletteChangeDivisor) % 255);
        cylon(leds, timeMs, color, width, periodMs, blurAmount);
    }});
//    _patterns.push_back({"testpattern", [](Leds& leds, int32_t timeMs) {
//        testpattern(leds, timeMs);
//    }});

    return;
}

static int16_t dist;  // A random number for our noise generator.
uint16_t xscale = 30;  // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint16_t yscale = 30;  // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint8_t maxChanges = 24;  // Value for blending between palettes.

CRGBPalette16 currentPalette(CRGB::White);

void pride(Leds& leds, int32_t timeMs, const CRGBPalette16& palette) 
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
    
//    CRGB newcolor = CHSV( hue8, sat8, bri8);
    CRGB newcolor = ColorFromPalette(palette,  hue8, bri8, LINEARBLEND);
    
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

// random colored speckles that blink in and fade smoothly
void confetti(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, uint8_t fade, uint8_t popChancePct) {                                             
  
    uint8_t   thisinc = 20;                     // Incremental value for rotating hues
    uint8_t   thissat = 255;                   // The saturation, where 255 = brilliant colours.
    uint8_t   thisbri = 200;                   // Brightness of a sequence. Remember, max_bright is the overall limiter.
    int       huediff = 100;                   // Range of random #'s to use for hue

    TBlendType currentBlending = LINEARBLEND_NOWRAP;
    fadeToBlackBy(leds.data(), kNumLeds, fade);                    // Low values = slower fade.
    int pos = random16(kNumLeds); 
    bool doPop = random16(100) > (100 - popChancePct);
    if (doPop) {
      leds[pos] = ColorFromPalette(palette,  thishue + random16(huediff)/4, thisbri, currentBlending);
    }
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

void firework(Leds& leds, int32_t timeMs, int periodMs, const CRGBPalette16& palette) {
  const auto fireworkFrac = timeFrac8(timeMs, periodMs);
  // Start with easeInVal at 0 and then go to 255 for the full easing.
  const uint8_t fireworkEased = easeOutQuart((float)fireworkFrac / 255) * 255; //ease8InOutCubic(count);

  // Map it to the number of LED's you have.
  const uint8_t fireworkLerpVal = lerp8by8(0, leds.size(), fireworkEased);

  uint8_t index = inoise8(0, dist + fireworkLerpVal * yscale) % 255;
  // With that value, look up the 8 bit colour palette value and assign it to the current LED.
  leds[fireworkLerpVal] = ColorFromPalette(palette, index, 255, LINEARBLEND);
  leds[fireworkLerpVal].maximizeBrightness();

  fadeToBlackBy(leds.data(), leds.size(), 16);  // 8 bit, 1 = slow fade, 255 = fast fade
#if 0
  if (fireworkFrac > 225) {
    for (int i = leds.size() - 20; i < leds.size(); i++) {
      index = inoise8(0, dist + fireworkLerpVal * yscale) % 255;
      leds[i] = ColorFromPalette(CRGBPalette16(CHSV(0, 255, 255),
                                               CHSV(40, 255, random(225, 255)),
                                               CHSV(80, 255, random(225, 255)),
                                               CHSV(140, 255, 255)), index, 255, LINEARBLEND);
      leds[i].fadeToBlackBy(1);
    }
  }
#endif
  
}

void testpattern(Leds& leds, int32_t timeMs) {
  const int kPeriodMs = 1500;
  const int fireworkCount = timeMs / kPeriodMs;
  auto timeEased = easeOutQuart(fireworkCount / 255.0f) * 255;
  auto lerpVal = lerp8by8(0, leds.size(), timeEased);
  return;
}

// Sliding bar across LEDs
void cylon(Leds& leds, int32_t timeMs, CRGB color, int width, int periodMs, fract8 blurAmount){
  const auto timeFrac = timeFrac8(timeMs, periodMs);
  const auto cylonFrac = quadwave8(timeFrac);
  const uint8_t lerpVal = lerp8by8(0, leds.size(), cylonFrac);

  for(int i = 0; i <= leds.size(); ++i) {
    const auto startLed = lerpVal - width / 2;
    const auto stopLed = lerpVal + width / 2;
    const auto isLit = (i >= startLed) && (i <= stopLed);
    if (isLit) {
      leds[i] = color;
    } else {
      leds[i] = CRGB::Black;
    }
  }
  blur1d( leds.data(), leds.size(), blurAmount);
}

void colorwaves( Leds& leds, int32_t timeMs, const CRGBPalette16& palette) 
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;
 
  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);
  
  uint16_t ms = timeMs;
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5,9);
  uint16_t brightnesstheta16 = sPseudotime;
  
  for( uint16_t i = 0 ; i < leds.size(); i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);
    
    uint8_t index = hue8;
    //index = triwave8( index);
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette( palette, index, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (leds.size()-1) - pixelnumber;
    
    nblend( leds[pixelnumber], newcolor, 128);
  }
}
#if 0
void lightning(Leds& leds, CRGB c, int simultaneous, int cycles, int speed){
  int flashes[simultaneous];

  for(int i=0; i<cycles; i++){
    for(int j=0; j<simultaneous; j++){
      int idx = random(leds.size());
      flashes[j] = idx;
      leds[idx] = c ? c : randomColor();
    }
    FastLED.show();
    delay(speed);
    for(int s=0; s<simultaneous; s++){
      leds[flashes[s]] = CRGB::Black;
    }
    delay(speed);
  }
}
#endif

} // namespace Scarf
