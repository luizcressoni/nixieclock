/*! \file rgb.h */
#pragma once

#include "pwm.h"
#include "../modulation/modulation.h"
#include <pthread.h>


class cRpiRgbh
{
  protected:
    cNixiePwm           *mpcNixiePwm[3]{};
    uint8_t             mu8pwm[3]{};
    pthread_t           mThreadt;
    bool                mTaskRunning;
    uint8_t             mu8pwmt{};
    bool                mIsModulationMine;
    cModulation         *mpModulation;
    enuModulationType   menuModulationType;
  public:
    cRpiRgbh(PIN_NAMES _r, PIN_NAMES _g, PIN_NAMES _b);
    virtual ~cRpiRgbh();
    virtual void    SetRgb(uint8_t _r, uint8_t _g, uint8_t _b);

    virtual void    StartThread();
    virtual void    StopThread();

    virtual bool    Task();

    virtual void    SetModulator(enuModulationType _enuModulationType, cModulation *_modulation = nullptr);
    virtual cModulation *GetModulatorPtr(){ return mpModulation;};
    virtual void SetModulationPeriod(uint32_t _u32period);
};


cRpiRgbh *init_rgb();
void deinit_rgb();
//eof rgb.h