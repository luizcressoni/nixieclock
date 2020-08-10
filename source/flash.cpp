/*! \file flash.cpp */
#include "modulation.h"
#include "functions.h"


double cFlash::GetValue()
{
    __u32 now = u32get_ms() - mu32resettime;
    __u32 n = now % (__u32)mdblperiodMs;
    return n>=(mdblperiodMs/2)?1.0:0.0;
}
