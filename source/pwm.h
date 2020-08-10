/*! \file pwm.h */
#pragma once

#include "gpio.h"
#include "pins.h"
#include "modulation.h"
#include <pthread.h>

class cNixiePwm
{
  protected:
//    cNeoGpioRaspi       *mpcNeoGpioRaspi;
    __u8                mu8pwm, mu8min, mu8max;
    pthread_t           mThreadt;
    __u8                mu8pwmpin,
                        mu8percent;
    __u32               mu32threadsleep;

    bool                mIsModulationMine;
    cModulation         *mpModulation;
    enuModulationType   menuModulationType;
  public:
    cNixiePwm(PIN_NAMES _pwmpin, __u8 _u8range=100);
    virtual ~cNixiePwm();
    virtual void        StartThread(void *_threadfunc(void *), __u32 _u32threadsleep_us);
    virtual void        StopThread();
    virtual bool        Task();

    virtual void        SetLimits(__u8 _u8min, __u8 _u8max);
    virtual void        SetPercent(__u8 _u8percent);
    virtual __u8        GetPercent(){ return mu8percent;};
    virtual void        SetAbsoluteValue(__u8 _u8value);
    virtual void        SetModulator(enuModulationType _enuModulationType, cModulation *_modulation = NULL);
    virtual cModulation *GetModulatorPtr(){ return mpModulation;};
};

//eof pwm.h
