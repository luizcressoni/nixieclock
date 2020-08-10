/*! \file ramp.h */
#pragma once

#include <linux/types.h>


class cModulation
{
  protected:
    __u32   mu32resettime;
    double  delta, b, mdblperiodMs;
    double  mdblvalue, mdbltarget;
    bool    rampUp;

  public:
    cModulation();
    //virtual ~cModulation();

    virtual void    Reset();
    virtual void    SetPeriod(__u32 _u32periodMs);
    virtual void    RampTo(double _value);
    virtual double  GetValue();
};

class cRamp : public cModulation
{
 protected:

 public:
    cRamp();
    virtual void    Reset();
    virtual void    SetPeriod(__u32 _u32periodMs);
    virtual void    RampTo(double _value);
    virtual double  GetValue();
};




//eof ramp.h
