/*! \file vu.cpp */
#include "vu.h"
#include <unistd.h>

/*! tested OK */

#define VU_THREAD_SLEEP     50
#define VU_MIN              15
#define VU_MAX              90

cNixiePwm *gNixieVu = NULL;

void *gVuThread(void *_data)
{
    cNixiePwm *aux = (cNixiePwm *)_data;
    while(aux->Task());
    return NULL;
}

cNixiePwm *init_vu()
{
    if(gNixieVu != NULL)
    {
        deinit_vu();
    }
    gNixieVu = new cNixiePwm(PIN_PWM_T);
    gNixieVu->SetLimits(VU_MIN,VU_MAX);
    gNixieVu->SetPercent(0);
    gNixieVu->StartThread(gVuThread, VU_THREAD_SLEEP);
    return gNixieVu;
}

void deinit_vu()
{
    delete gNixieVu;
}
//eof vu.cpp
