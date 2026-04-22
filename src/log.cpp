#include "log.h"

#include <stdio.h>
#include <stdarg.h>

#if SCARFNET_EMBEDDED
#include <Arduino.h>
static unsigned long _logMillis() { return millis(); }
#else
#include <chrono>
static unsigned long _logMillis() {
    using namespace std::chrono;
    return (unsigned long)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}
#endif

namespace Scarfnet
{

void log( const char *msg , ... )
{
#if SCARFNET_EMBEDDED
   printf("[T+%lu] ", _logMillis());
   va_list args;
   va_start( args, msg );
   vprintf( msg, args );
   va_end( args );
   putchar('\n');
#else
   (void)msg;  // suppress output in native/sim builds
#endif
}

}
