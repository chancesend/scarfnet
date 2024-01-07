#pragma once 

#if SCARFNET_EMBEDDED
//#define FASTLED_INTERNAL (1)
#include <FastLED.h>
#endif

namespace Scarfnet
{

CRGBPalette16 getColorPalette(int8_t i);

} // namespace Scarfnet
