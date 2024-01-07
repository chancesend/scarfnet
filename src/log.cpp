#include "log.h"

#include <stdio.h>
#include <stdarg.h>

#include <HardwareSerial.h>

namespace Scarfnet
{

void log( const char *msg , ... )
{
   va_list args;
   va_start( args, msg );
   Serial.printf( msg, args );
   va_end( args );
}

}
