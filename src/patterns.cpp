#include "patterns.h"
#include "defines.h"

#include <stdint.h>

static int16_t dist;  // A random number for our noise generator.
uint16_t xscale = 30;  // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint16_t yscale = 30;  // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint8_t maxChanges = 24;  // Value for blending between palettes.

CRGBPalette16 currentPalette(CRGB::White);

void pride(led_list& leds) 
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
  
  uint16_t ms = millis();
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

void fillnoise8(led_list& leds, int32_t time) {
  // Just ONE loop to fill up the LED array as all of the pixels change.
  for(int i = 0; i < kNumLeds; i++) {
    // Get a value from the noise function. I'm using both x and y axis.
    uint8_t index = inoise8(0, dist + i * yscale) % 255;
    // With that value, look up the 8 bit colour palette value and assign it to the current LED.
    leds[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);
  }
    dist += beatsin8(10, 1, 4, time);                        
}

void fillNoise(led_list& leds, int32_t time) {
  fillnoise8(leds, time);
}

uint8_t  thisfade = 8;                     // How quickly does it fade? Lower = slower fade rate.
int       thishue = 192;                   // Starting hue.
uint8_t   thisinc = 2;                     // Incremental value for rotating hues
uint8_t   thissat = 255;                   // The saturation, where 255 = brilliant colours.
uint8_t   thisbri = 255;                   // Brightness of a sequence. Remember, max_bright is the overall limiter.
int       huediff = 256;                   // Range of random #'s to use for hue


void confetti(led_list& leds) {                                             // random colored speckles that blink in and fade smoothly
    TBlendType currentBlending = LINEARBLEND_NOWRAP;
    fadeToBlackBy(leds.data(), kNumLeds, thisfade);                    // Low values = slower fade.
    int pos = random16(kNumLeds); 
    leds[pos] = ColorFromPalette(currentPalette,  thishue + random16(huediff)/4, thisbri, currentBlending);
    thishue = thishue + thisinc;                                   // It increments here.
}