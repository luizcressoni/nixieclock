/*! \file rgb.h */
#pragma once

#include "pwm.h"
#include "modulation.h"
#include <pthread.h>


class cNeoRpiRgbh
{
  protected:
    cNixiePwm           *mpcNixiePwm[3];
    __u8                mu8pwm[3];
    pthread_t           mThreadt;
    bool                mTaskRunning;
    __u8                mu8pwmt;
    bool                mIsModulationMine;
    cModulation         *mpModulation;
    enuModulationType   menuModulationType;
  public:
    cNeoRpiRgbh(PIN_NAMES _r, PIN_NAMES _g, PIN_NAMES _b);
    virtual ~cNeoRpiRgbh();
    virtual void    SetRgb(__u8 _r, __u8 _g, __u8 _b);

    virtual void    StartThread();
    virtual void    StopThread();

    virtual bool    Task();

    virtual void    SetModulator(enuModulationType _enuModulationType, cModulation *_modulation);
    virtual cModulation *GetModulatorPtr(){ return mpModulation;};
};

/*
class cNeoRpiRgbhPIC
{
  protected:
    __u8            mu8vector[5];
    cNeoRpiSerial   *mpcNeoRpiSerial;
  public:
    cNeoRpiRgbhPIC();
    virtual ~cNeoRpiRgbhPIC();
    virtual void    SetRgb(__u8 _r, __u8 _g, __u8 _b);
    virtual void    SetH(__u8 _h);
};
*/

//eof rgb.h
