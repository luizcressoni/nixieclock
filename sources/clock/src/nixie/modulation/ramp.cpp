/*!\file ramp.cpp */
#include "modulation.h"



void cRamp::RampTo(double _value)
{
    Reset();
    if(_value == 0.0)
        mdblvalue = 1.0;
    rampUp = (_value > mdblvalue);
    mdbltarget = _value;
    delta = (_value - mdblvalue) / mdblperiodMs;
    b = mdblvalue;
}

double cRamp::GetValue()
{
    uint32_t now = u32get_ms() - mu32resettime;
    mdblvalue = delta * now + b;

    if((rampUp && (mdbltarget < mdblvalue)) ||
       (!rampUp && (mdbltarget > mdblvalue)))
    {
        mdblvalue = mdbltarget;
    }

    return mdblvalue;
}


//eof ramp.cpp
