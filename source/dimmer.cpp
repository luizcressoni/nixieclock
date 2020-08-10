/*! \file dimmer.cpp */
#include "dimmer.h"
#include <unistd.h>

/*! tested OK */

#define DIMMER_THREAD_SLEEP     50
#define DIMMER_MIN              100     //! inverted min/max is OK
#define DIMMER_MAX              0       //! High Voltage pwm is inverted

cNixiePwm *gNixieDimmer = NULL;

void *gDimmerThread(void *_data)
{
    cNixiePwm *aux = (cNixiePwm *)_data;
    while(aux->Task());
    return NULL;
}

cNixiePwm *init_dimmer()
{
    if(gNixieDimmer != NULL)
    {
        deinit_dimmer();
    }
    gNixieDimmer = new cNixiePwm(PIN_PWM_LAMPS);
    gNixieDimmer->SetLimits(DIMMER_MIN, DIMMER_MAX);
    gNixieDimmer->SetPercent(0);
    gNixieDimmer->StartThread(gDimmerThread, DIMMER_THREAD_SLEEP);
    return gNixieDimmer;
}

void deinit_dimmer()
{
    delete gNixieDimmer;
}
