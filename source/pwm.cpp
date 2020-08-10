/*! \file pwm.cpp */
#include "pwm.h"
#include <unistd.h>
#include <stdio.h>

#include <pigpio.h>

cNixiePwm::cNixiePwm(PIN_NAMES _pwmpin, __u8 _u8range)
{
    mThreadt = 0;
    mu8pwmpin = _pwmpin;
    mu8percent = 0;
    mu32threadsleep = 0;
    mIsModulationMine = false;
    mpModulation = NULL;
    SetLimits(0,_u8range);
    gpioSetPWMrange(mu8pwmpin, _u8range);
    SetAbsoluteValue(0);
}

cNixiePwm::~cNixiePwm()
{
    StopThread();
    SetAbsoluteValue(0);
    if(mIsModulationMine && mpModulation != NULL)
    {
        delete mpModulation;
    }
}

void cNixiePwm::SetLimits(__u8 _u8min, __u8 _u8max)
{
    mu8min = _u8min;
    mu8max = _u8max;
    SetPercent(mu8percent);
}

void cNixiePwm::SetAbsoluteValue(__u8 _u8value)
{
    gpioPWM(mu8pwmpin, _u8value);
}

void cNixiePwm::SetPercent(__u8 _u8percent)
{
    double y, yi, yf;
    if(_u8percent > 100)
        _u8percent = 100;
    mu8percent = _u8percent;
    yi = mu8min;
    yf = mu8max;
    y = yi + _u8percent * (yf - yi) / 100.0;
    mu8pwm = y;

    gpioPWM(mu8pwmpin, mu8pwm);
}

void cNixiePwm::StartThread(void *_threadfunc(void *), __u32 _u32threadsleep_us)
{
    if(mThreadt == 0 && mu32threadsleep == 0)
    {
        mu32threadsleep = _u32threadsleep_us;
        if(pthread_create(&mThreadt, NULL, _threadfunc, this) != 0)
        {
            mu32threadsleep = 0;
            mThreadt = 0;
        }
    }
}

void cNixiePwm::StopThread()
{
    gpioPWM(mu8pwmpin, 0);
    if(mThreadt != 0 && mu32threadsleep > 0)
    {
        mu32threadsleep = 0;
        pthread_join(mThreadt, NULL);
    }
}


bool cNixiePwm::Task()
{
    return true;
    double dvalue = mu8pwm;
    if(mpModulation != NULL)
    {
        dvalue *= mpModulation->GetValue();
    }
    if(mu32threadsleep)
        usleep(mu32threadsleep);
    return mu32threadsleep > 0;
}


void cNixiePwm::SetModulator(enuModulationType _enuModulationType, cModulation *_modulation)
{
    menuModulationType = _enuModulationType;
    if(_modulation == NULL)
    {
        switch(menuModulationType)
        {
            case enuModulationTypeRamp:
                mpModulation = new cRamp();
                mIsModulationMine = true;
                break;
            case enuModulationTypeSinusoidal:
                mpModulation = new cSinusoidal();
                mIsModulationMine = true;
                break;
            case enuModulationTypeFlash:
                mpModulation = new cFlash();
                mIsModulationMine = true;
                break;
            case enuModulationTypeNone:
                if(mpModulation != NULL && mIsModulationMine)
                {
                    delete mpModulation;
                    mpModulation = NULL;
                    mIsModulationMine = false;
                }
                break;
        }
    }
    else
    {
        if(mIsModulationMine && mpModulation != NULL)
        {
            delete mpModulation;
        }
        mIsModulationMine = false;
        mpModulation = _modulation;
    }
}

//eof pwm.cpp
