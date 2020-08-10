/*!\file modulation.cpp */
#include "modulation.h"
#include "functions.h"


cModulation::cModulation()
{
    Reset();
    mdblvalue = 0.0;
    SetPeriod(1000);
    RampTo(1.0);
}

cModulation::~cModulation()
{

}

void cModulation::Reset()
{
    mu32resettime = u32get_ms();
}

void cModulation::SetPeriod(__u32 _u32periodMs)
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
