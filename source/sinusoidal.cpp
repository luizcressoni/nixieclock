/*! \file sinusoidal.cpp */
#include "modulation.h"
#include <math.h>
#include "functions.h"

void cSinusoidal::SetPeriod(__u32 _u32periodMs)
{
    w = 2.0 * M_PI / _u32periodMs;
}

double cSinusoidal::GetValue()
{
    __u32 t = u32get_ms() - mu32resettime;
    return 0.5 * sin(w * t) + 0.5;
}

//eof sinusoidal.cpp
