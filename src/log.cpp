#include "log.h"

#include <stdio.h>
#include <stdarg.h>

#include <Arduino.h>

namespace Scarfnet
{

void log( const char *msg , ... )
{
   printf("[T+%lu] ", (unsigned long)millis());
   va_list args;
   va_start( args, msg );
   vprintf( msg, args );
   va_end( args );
   putchar('\n');
}

}
