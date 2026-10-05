/*! \file vu.cpp */
#include "vu.h"

#include "pins.h"

/*! tested OK */

#define VU_THREAD_SLEEP     50

cNixiePwm *gNixieVu = nullptr;

void *gVuThread(void *_data)
{
    auto *aux = static_cast<cNixiePwm *>(_data);
    while(aux->Task());
    return nullptr;
}

cNixiePwm *init_vu(int _min, int _max)
{
    if(gNixieVu == nullptr)
    {
        gNixieVu = new cNixiePwm(PIN_PWM_T);
        gNixieVu->StartThread(gVuThread, VU_THREAD_SLEEP);
        gNixieVu->SetPercent(0);
    }
    gNixieVu->SetLimits(_min,_max);
    return gNixieVu;
}

void deinit_vu()
{
    gNixieVu->SetPercent(0);
    delete gNixieVu;
}
//eof vu.cpp
