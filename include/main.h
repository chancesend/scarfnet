#pragma once

#include "defines.h"

#if SCARFNET_EMBEDDED
#include <Arduino.h>
//#include <M5Stack.h>
//#include <M5Atom.h>
#endif

#include <stdint.h>

uint32_t get_millisecond_timer();