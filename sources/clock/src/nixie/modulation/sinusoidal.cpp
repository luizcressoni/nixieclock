/*! \file sinusoidal.cpp */
#include "modulation.h"
#include <cmath>

void cSinusoidal::SetPeriod(uint32_t _u32periodMs)
{
    w = 2.0 * M_PI / _u32periodMs;
}

double cSinusoidal::GetValue()
{
    const uint32_t t = u32get_ms() - mu32resettime;
    return 0.5 * sin(w * t) + 0.5;
}

//eof sinusoidal.cpp
