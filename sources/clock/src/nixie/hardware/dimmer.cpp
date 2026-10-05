/*! \file dimmer.cpp */
#include "dimmer.h"
#include "pins.h"

#define DIMMER_THREAD_SLEEP     50


cNixiePwm *gNixieDimmer = nullptr;

void *gDimmerThread(void *_data)
{
    auto *aux = static_cast<cNixiePwm *>(_data);
    while(aux->Task());
    return nullptr;
}

/*! \brief Initializes the dimmer with the specified maximum value.
    \param _max The maximum value for the dimmer.
    \return A pointer to the initialized cNixiePwm object.
    \note If the dimmer is already initialized, it will not be re-initialized.
*/
cNixiePwm *init_dimmer(int _max)
{
    //printf("Initializing dimmer with min=%d, max=%d\n", _min, _max);
    if(gNixieDimmer == nullptr)
    {
        gNixieDimmer = new cNixiePwm(PIN_PWM_LAMPS);
        gNixieDimmer->SetLimits(_max);
        gNixieDimmer->SetPercent(100);
        gNixieDimmer->StartThread(gDimmerThread, DIMMER_THREAD_SLEEP);
    }
    return gNixieDimmer;
}

/*! \brief Deinitializes the dimmer.
    \note Stops the modulation and sets the dimmer to 0% before deleting the cNixiePwm object.
    \note If the dimmer is not initialized, this function does nothing.
*/
void deinit_dimmer()
{
    gNixieDimmer->SetModulator(enuModulationTypeNone); 
    gNixieDimmer->SetPercent(0);
    delete gNixieDimmer;
    gNixieDimmer = nullptr;
}

//eof dimmer.cpp