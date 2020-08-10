/*! \file modulation.h */
#pragma once

#include <linux/types.h>

enum enuModulationType
{
    enuModulationTypeRamp,
    enuModulationTypeSinusoidal,
    enuModulationTypeFlash,
    enuModulationTypeNone
};

class cModulation
{
  protected:
    __u32   mu32resettime;
    double  delta, b, w, mdblperiodMs;
    double  mdblvalue, mdbltarget;
    bool    rampUp;

  public:
    cModulation();
    virtual ~cModulation();
    virtual void    Reset();
    virtual void    SetPeriod(__u32 _u32periodMs);
    virtual void    RampTo(double _value);
    virtual double  GetValue();
};

class cRamp : public cModulation
{
 protected:

 public:
//    cRamp();
//    void    SetPeriod(__u32 _u32periodMs);
    void    RampTo(double _value);
    double  GetValue();
};

class cSinusoidal : public cModulation
{
 protected:

 public:
//    cSinusoidal();
    void    SetPeriod(__u32 _u32periodMs);
    double  GetValue();
};


class cFlash : public cModulation
{
 public:
    double  GetValue();
};

//eof modulation.h

