#include <stdio.h>
#include "unity_config.h"

void unityOutputStart(unsigned long a) {}

void unityOutputChar(unsigned int c) {
    printf("%c", c);
}

void unityOutputFlush() {}

void unityOutputComplete() {}
