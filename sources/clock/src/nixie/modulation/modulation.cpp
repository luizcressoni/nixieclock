/*!\file modulation.cpp */
#include "modulation.h"
#include <cstdint>
#include <sys/time.h>
#include <ctime>


uint32_t cModulation::u32get_ms()
{
    timeval tv{};
    gettimeofday(&tv, nullptr);
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

cModulation::cModulation()
{
    Reset();
    mdblvalue = 0.0;
    SetPeriod(1000);
    RampTo(1.0);
}

cModulation::~cModulation()
= default;

void cModulation::Reset()
{
    mu32resettime = u32get_ms();
}

void cModulation::SetPeriod(uint32_t _u32periodMs)
{
    mdblperiodMs = _u32periodMs;
}

void cModulation::RampTo(double _value)
{

}

double cModulation::GetValue()
{
    return mdblvalue;
}


//eof modulation.cpp
