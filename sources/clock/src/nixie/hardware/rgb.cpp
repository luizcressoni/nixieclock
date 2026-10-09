/*! \file rgb.cpp */

#include "rgb.h"
#include <unistd.h>
#include "pins.h"

#define RGB_THREAD_SLEEP    10000

cRpiRgbh *gRpiRgbh = nullptr;

cRpiRgbh *init_rgb()
{
    gRpiRgbh = new cRpiRgbh(PIN_RGB_R, PIN_RGB_G, PIN_RGB_B);
    gRpiRgbh->StartThread();
    gRpiRgbh->SetRgb(0,0,0);
    return gRpiRgbh;
}

void deinit_rgb()
{
    gRpiRgbh->SetRgb(0,0,0);
    delete gRpiRgbh;
}

void *gRgbThread(void *_data)
{
    auto aux = static_cast<cRpiRgbh *>(_data);
    while(aux->Task())
    {
        usleep(RGB_THREAD_SLEEP);
    }
    return nullptr;
}

cRpiRgbh::cRpiRgbh(PIN_NAMES _r, PIN_NAMES _g, PIN_NAMES _b)
{
    mpcNixiePwm[0] = _r == GPIO_NONE? nullptr : new cNixiePwm(_r, 255);
    mpcNixiePwm[1] = _g == GPIO_NONE? nullptr : new cNixiePwm(_g, 255);
    mpcNixiePwm[2] = _b == GPIO_NONE? nullptr : new cNixiePwm(_b, 255);

    mThreadt = 0;
    mTaskRunning = false;

    mIsModulationMine = false;
    mpModulation = nullptr;

    for(auto & i : mpcNixiePwm)
    {
        if(i != nullptr)
        {
            i->SetAbsoluteValue(255);
        }
    }

}

cRpiRgbh::~cRpiRgbh()
{
    StopThread();

    if(mIsModulationMine && mpModulation != nullptr)
        delete mpModulation;

    for(const auto & i : mpcNixiePwm)
        delete i;
}

void cRpiRgbh::SetRgb(uint8_t _r, uint8_t _g, uint8_t _b)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mu8pwm[0] = _r;
    mu8pwm[1] = _g;
    mu8pwm[2] = _b;

    for(int i=0;i<3;i++)
    {
        if(mpcNixiePwm[i] != nullptr)
        {
            mpcNixiePwm[i]->SetAbsoluteValue(255 - mu8pwm[i]);
        }
    }
}

void cRpiRgbh::StartThread()
{
    if(mThreadt == 0 && !mTaskRunning)
    {
        mTaskRunning = true;
        if(pthread_create(&mThreadt, nullptr, gRgbThread, this) != 0)
        {
            mTaskRunning = false;
            mThreadt = 0;
        }
    }
}

void cRpiRgbh::StopThread()
{
    if(mThreadt != 0 && mTaskRunning)
    {
        mTaskRunning = false;
        pthread_join(mThreadt, nullptr);
    }
}

bool cRpiRgbh::Task()
{
    std::lock_guard<std::mutex> lock(mMutex);
    double dvalue = 1.0;
    if(mpModulation != nullptr)
    {
        dvalue = mpModulation->GetValue();
    }

    for(int i=0;i<3;i++)
    {
        if(mpcNixiePwm[i] != nullptr)
        {
            mpcNixiePwm[i]->SetAbsoluteValue(255 - mu8pwm[i] * dvalue);
        }
    }
    return mTaskRunning;
}


void cRpiRgbh::SetModulator(enuModulationType _enuModulationType, cModulation *_modulation)
{
    cModulation *fresh = _modulation;
    if(fresh == nullptr)
    {
        switch(_enuModulationType)
        {
            case enuModulationTypeRamp:         fresh = new cRamp();        break;
            case enuModulationTypeSinusoidal:   fresh = new cSinusoidal();  break;
            case enuModulationTypeFlash:        fresh = new cFlash();       break;
            default:                                                        break;
        }
    }

    std::lock_guard<std::mutex> lock(mMutex);
    if(mIsModulationMine && mpModulation != nullptr && mpModulation != fresh)
        delete mpModulation;
    menuModulationType = _enuModulationType;
    mpModulation = fresh;
    mIsModulationMine = (fresh != nullptr && _modulation == nullptr);
}


void cRpiRgbh::SetModulationPeriod(uint32_t _u32period)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if(mpModulation)
        mpModulation->SetPeriod(_u32period);
}

//eof rgb.cpp