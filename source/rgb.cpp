/*! \file rgb.cpp */

#include "rgb.h"
#include <unistd.h>
#include <stdio.h>

#define RGB_THREAD_SLEEP    10000

void *gRgbThread(void *_data)
{
    cNeoRpiRgbh *aux = (cNeoRpiRgbh *)_data;
    while(aux->Task())
    {
        usleep(RGB_THREAD_SLEEP);
    }
    return NULL;
}

cNeoRpiRgbh::cNeoRpiRgbh(PIN_NAMES _r, PIN_NAMES _g, PIN_NAMES _b)
{
    mpcNixiePwm[0] = _r == GPIO_NONE? NULL : new cNixiePwm(_r, 255);
    mpcNixiePwm[1] = _g == GPIO_NONE? NULL : new cNixiePwm(_g, 255);
    mpcNixiePwm[2] = _b == GPIO_NONE? NULL : new cNixiePwm(_b, 255);

    mThreadt = 0;
    mTaskRunning = false;

    mIsModulationMine = false;
    mpModulation = NULL;

    for(__u8 i=0;i<3;i++)
    {
        if(mpcNixiePwm[i] != 0)
        {
            mpcNixiePwm[i]->SetAbsoluteValue(255);
        }
    }

}

cNeoRpiRgbh::~cNeoRpiRgbh()
{
    StopThread();

    if(mIsModulationMine && mpModulation != NULL)
    {
        delete mpModulation;
    }

    for(__u8 i=0;i<3;i++)
    {
        if(mpcNixiePwm[i] != NULL)
            delete mpcNixiePwm[i];
    }
}

void cNeoRpiRgbh::SetRgb(__u8 _r, __u8 _g, __u8 _b)
{
    mu8pwm[0] = _r;
    mu8pwm[1] = _g;
    mu8pwm[2] = _b;

    for(__u8 i=0;i<3;i++)
    {
        if(mpcNixiePwm[i] != NULL)
        {
            mpcNixiePwm[i]->SetAbsoluteValue(255 - mu8pwm[i]);
        }
    }
}

void cNeoRpiRgbh::StartThread()
{
    if(mThreadt == 0 && !mTaskRunning)
    {
        mTaskRunning = true;
        if(pthread_create(&mThreadt, NULL, gRgbThread, this) != 0)
        {
            mTaskRunning = false;
            mThreadt = 0;
        }
    }
}

void cNeoRpiRgbh::StopThread()
{
    if(mThreadt != 0 && mTaskRunning)
    {
        mTaskRunning = false;
        pthread_join(mThreadt, NULL);
    }
}

bool cNeoRpiRgbh::Task()
{
    double dvalue = 1.0;
    if(mpModulation != NULL)
    {
        dvalue = mpModulation->GetValue();
        //printf("dvalue=%0.2f\n", dvalue);
    }

    for(__u8 i=0;i<3;i++)
    {
        if(mpcNixiePwm[i] != NULL)
        {
            mpcNixiePwm[i]->SetAbsoluteValue(255 - mu8pwm[i] * dvalue);
        }
    }
//    if(++mu8pwmt >= mu8RgbMax)
//        mu8pwmt = 0;
    return mTaskRunning;
}


void cNeoRpiRgbh::SetModulator(enuModulationType _enuModulationType, cModulation *_modulation)
{
    menuModulationType = _enuModulationType;
    if(_modulation == NULL)
    {
        switch(menuModulationType)
        {
            case enuModulationTypeRamp:
                mIsModulationMine = true;
                mpModulation = new cRamp();
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
        mpModulation = _modulation;
    }
}


/****************************************************************/
/****************************************************************/
/****************************************************************/
/****************************************************************/
/*

cNeoRpiRgbhPIC::cNeoRpiRgbhPIC()
{
    mpcNeoRpiSerial = new cNeoRpiSerial("9600:8N1");
    mu8vector[0] = '*';
    for(__u8 i=1;i<5;i++)
    {
        mu8vector[i] = 0;
    }
}

cNeoRpiRgbhPIC::~cNeoRpiRgbhPIC()
{
    delete mpcNeoRpiSerial;
}

void cNeoRpiRgbhPIC::SetRgb(__u8 _r, __u8 _g, __u8 _b)
{
    mu8vector[1] = _r>100?100:_r;
    mu8vector[2] = _g>100?100:_g;
    mu8vector[3] = _b>100?100:_b;
    mpcNeoRpiSerial->SendWord(mu8vector,5);
}

void cNeoRpiRgbhPIC::SetH(__u8 _h)
{
    mu8vector[4] = _h>100?100:_h;
    mpcNeoRpiSerial->SendWord(mu8vector,5);
}
*/

//eof rgb.cpp
