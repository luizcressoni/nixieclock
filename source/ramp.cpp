/*!\file ramp.cpp */
#include "modulation.h"
#include "functions.h"


void cRamp::RampTo(double _value)
{
    Reset();
    rampUp = (_value > mdblvalue);
    mdbltarget = _value;
    delta = (_value - mdblvalue) / mdblperiodMs;
    b = mdblvalue;
}

double cRamp::GetValue()
{
    __u32 now = u32get_ms() - mu32resettime;
    mdblvalue = delta * now + b;

    if((rampUp && (mdbltarget < mdblvalue)) ||
       (!rampUp && (mdbltarget > mdblvalue)))
    {
        mdblvalue = mdbltarget;
    }

    return mdblvalue;
}


//eof ramp.cpp
