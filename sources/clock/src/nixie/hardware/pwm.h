/*! \file pwm.h */
#pragma once

#include "gpio.h"
#include "../modulation/modulation.h"
#include "../../utils/ctimer.h"
#include <pthread.h>

class cNixiePwm
{
  protected:
    uint8_t             mu8pwm{}, mu8min{}, mu8max{};
    pthread_t           mThreadt;
    uint8_t             mu8pwmpin,
                        mu8percent;
    uint32_t            mu32threadsleep;

    //for linear interpolation
    double              a{}, b{};

    bool                mIsModulationMine;
    bool                mIsModulationOneShot;
    cModulation         *mpModulation;
    enuModulationType   menuModulationType;

    cTimer              mTimerOff,
                        mTimerForced;
  public:
    explicit cNixiePwm(PIN_NAMES _pwmpin, uint8_t _u8range=100);
    virtual ~cNixiePwm();
    virtual void        StartThread(void *_threadfunc(void *), uint32_t _u32threadsleep_us);
    virtual void        StopThread();
    virtual bool        Task();

    virtual void        SetLimits(uint8_t _u8min, uint8_t _u8max);
    virtual void        SetLimits(uint8_t _u8max);
    virtual void        SetPercent(uint8_t _u8percent);
    virtual void        SetPercent(uint8_t _u8percent, uint32_t _u32timeoff);
    virtual uint8_t     GetPercent(){ return mu8percent;};
    virtual void        SetAbsoluteValue(uint8_t _u8value);
    virtual void        SetPhysicalZero();
    virtual void        SetModulator(enuModulationType _enuModulationType, cModulation *_modulation = nullptr);
    virtual cModulation *GetModulatorPtr(){ return mpModulation;};

    virtual void        SetMinimum(uint8_t _u8min, uint32_t _u32timeForced);
    virtual void        SetMaximum(uint8_t _u8max, uint32_t _u32timeForced);

    virtual void        SetModulationPeriod(uint32_t _u32period);
};

//eof pwm.h
