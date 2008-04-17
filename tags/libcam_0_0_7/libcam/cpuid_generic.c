#include "cpuid.h"

void
cpuid_detect (int * sse2, int * sse3)
{
    if (sse2)
        *sse2 = 0;
    if (sse3)
        *sse3 = 0;
}
