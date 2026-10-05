/*! \file flash.cpp */
#include "modulation.h"
#include <cstdint>

double cFlash::GetValue()
{
    uint32_t now = u32get_ms() - mu32resettime;
    uint32_t n = now % static_cast<uint32_t>(mdblperiodMs);
    return n>=(mdblperiodMs/2)?1.0:0.0;
}
