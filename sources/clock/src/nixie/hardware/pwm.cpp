/*! \file pwm.cpp */
#include "pwm.h"
#include <unistd.h>
#include <cstdio>

#include <pigpio.h>

cNixiePwm::cNixiePwm(PIN_NAMES _pwmpin, uint8_t _u8range)
{
    mThreadt = 0;
    mu8pwmpin = _pwmpin;
    mu8percent = 0;
    mu32threadsleep = 0;
    mIsModulationMine = false;
    mIsModulationOneShot = false;
    mpModulation = nullptr;
    SetLimits(0,_u8range);
    gpioSetPWMrange(mu8pwmpin, _u8range);
    SetAbsoluteValue(0);
    mTimerOff.Enable(false);
    mTimerForced.Enable(false);
}

cNixiePwm::~cNixiePwm()
{
    if(mIsModulationMine && mpModulation != nullptr)
    {
        delete mpModulation;
        mpModulation = nullptr;
    }
    StopThread();
    SetAbsoluteValue(mu8min);
}

void cNixiePwm::SetLimits(uint8_t _u8max)
{
    if(_u8max > 100)
        _u8max = 100;
    SetLimits(100, 100 - _u8max);
}

void cNixiePwm::SetLimits(uint8_t _u8min, uint8_t _u8max)
{
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    mu8min = _u8min;
    mu8max = _u8max;
    a = (mu8max - mu8min) / 100.0;
    b = mu8min;
    //printf("Setting limits: min=%d, max=%d -> a=%f, b=%f\n", mu8min, mu8max, a, b);
    SetPercent(mu8percent);
}

void cNixiePwm::SetAbsoluteValue(uint8_t _u8value)
{
    gpioPWM(mu8pwmpin, _u8value);
}

/*! \brief Drives the output to a real 0, bypassing the calibrated limits
    \note SetPercent(0) writes the minimum from SetLimits, and on the bargraph that minimum is
    \note calibrated to sit at the very bottom of the scale, which still glows a little. This is
    \note for when the output must really be off. Modulation and the off timer are stopped too,
    \note or the PWM thread would write the calibrated minimum back on its next pass.
*/
void cNixiePwm::SetPhysicalZero()
{
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    SetModulator(enuModulationTypeNone, nullptr);
    mTimerOff.Enable(false);
    mu8percent = 0;
    mu8pwm = 0;
    gpioPWM(mu8pwmpin, 0);
}

void cNixiePwm::SetPercent(uint8_t _u8percent, uint32_t _u32timeoff)
{
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    SetPercent(_u8percent);
    if(_u32timeoff != 0)
    {
        //printf("Setting timeout to %dms\n", _u32timeoff);
        mTimerOff.SetTimeOut(_u32timeoff);
    }
}

void cNixiePwm::SetPercent(uint8_t _u8percent)
{
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    if(_u8percent > 100)
        _u8percent = 100;
    mu8percent = _u8percent;
    double y = a * _u8percent + b;
    //printf("Percent: %d --> %f\n", _u8percent, y);
    mu8pwm = static_cast<uint8_t>(y);
    gpioPWM(mu8pwmpin, mu8pwm);
}

void cNixiePwm::StartThread(void *_threadfunc(void *), uint32_t _u32threadsleep_us)
{
    //printf("PWM thread ON\n");
    if(mThreadt == 0 && mu32threadsleep == 0)
    {
        mu32threadsleep = _u32threadsleep_us;
        if(pthread_create(&mThreadt, nullptr, _threadfunc, this) != 0)
        {
            mu32threadsleep = 0;
            mThreadt = 0;
        }
    }
}

void cNixiePwm::StopThread()
{
    //printf("PWM thread stop\n");
    gpioPWM(mu8pwmpin, 0);
    if(mThreadt != 0 && mu32threadsleep > 0)
    {
        mu32threadsleep = 0;
        pthread_join(mThreadt, nullptr);
    }
}


bool cNixiePwm::Task()
{
    {
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    if(mTimerForced.IsEnabled())
    {
        if(mTimerForced.IsTimeOut())
        {
            mTimerForced.Enable(false);
        }
        return mu32threadsleep > 0;
    }

    if(mpModulation != nullptr)
    {
        double dvalue = mpModulation->GetValue();
        SetPercent(dvalue * 100.0);
        if(mIsModulationOneShot)
        {
            if((menuModulationType == enuModulationTypeRampOneShotDown && dvalue <= 0.0) || 
                (menuModulationType == enuModulationTypeRampOneShotUp && dvalue >= 1.0))
            {
                SetModulator(enuModulationTypeNone);
                //printf("Modulation ended\n");
            }
        }
    }


    if(mTimerOff.IsTimeOut())
    {
        SetPercent(0);
        mTimerOff.Enable(false);
    }
    }   //not held over the sleep below

    if(mu32threadsleep)
        usleep(mu32threadsleep);

    return mu32threadsleep > 0;
}


/*! \brief Builds one of the stock modulations, fully set up
    \note A fresh cRamp reads 0 until RampTo() is called on it. Published half built, the PWM
    \note thread could read that 0, take a ramp down as already finished and remove it.
*/
cModulation *cNixiePwm::NewModulation(enuModulationType _type)
{
    switch(_type)
    {
        case enuModulationTypeRampOneShotDown:
        case enuModulationTypeRampOneShotUp:
        {
            auto *ramp = new cRamp();
            ramp->SetPeriod(2000); // 2 seconds
            ramp->RampTo(_type == enuModulationTypeRampOneShotDown ? 0.0 : 1.0);
            return ramp;
        }
        case enuModulationTypeRamp:         return new cRamp();
        case enuModulationTypeSinusoidal:   return new cSinusoidal();
        case enuModulationTypeFlash:        return new cFlash();
        default:                            return nullptr;
    }
}

void cNixiePwm::SetModulator(enuModulationType _enuModulationType, cModulation *_modulation)
{
    //built outside the lock: the PWM thread need not wait for an allocation
    cModulation *fresh = (_modulation == nullptr) ? NewModulation(_enuModulationType) : _modulation;

    std::lock_guard<std::recursive_mutex> lock(mMutex);
    //the one being replaced used to be dropped without a delete, every time a ramp started
    if(mIsModulationMine && mpModulation != nullptr && mpModulation != fresh)
        delete mpModulation;
    menuModulationType = _enuModulationType;
    mpModulation = fresh;
    mIsModulationMine = (fresh != nullptr && _modulation == nullptr);
    mIsModulationOneShot = mIsModulationMine &&
                           (_enuModulationType == enuModulationTypeRampOneShotDown ||
                            _enuModulationType == enuModulationTypeRampOneShotUp);
}

void cNixiePwm::SetMinimum(uint8_t _u8min, uint32_t _u32timeForced)
{
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    SetModulator(enuModulationTypeNone, nullptr);
    SetLimits(_u8min, mu8max);
    mTimerForced.SetTimeOut(_u32timeForced);
    mTimerForced.Enable(true);
    SetPercent(0);
}

void cNixiePwm::SetMaximum(uint8_t _u8max, uint32_t _u32timeForced)
{
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    SetModulator(enuModulationTypeNone, nullptr);
    SetLimits(mu8min, _u8max);
    mTimerForced.SetTimeOut(_u32timeForced);
    mTimerForced.Enable(true);
    SetPercent(100);
}


void cNixiePwm::SetModulationPeriod(uint32_t _u32period)
{
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    if(mpModulation)
        mpModulation->SetPeriod(_u32period);
}
//eof pwm.cpp
