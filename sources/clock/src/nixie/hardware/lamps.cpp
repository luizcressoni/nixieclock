/*! \file lamps.cpp */
#include "lamps.h"
#include "pins.h"

/*! \brief Constructor
    \note Opens the three GPIO lamps for HMS, Temperature, and Weekday.
*/
cNixieLamps::cNixieLamps()
{
    mGpioHms = new cGpioRaspi(PIN_LAMP_HMS, OUTPUT_PIN);
    mGpioTemp = new cGpioRaspi(PIN_LAMP_TEMP, OUTPUT_PIN);
    mGpioWeekday = new cGpioRaspi(PIN_LAMP_WDAY, OUTPUT_PIN);
    
    mGpioHms->SetValue(PIN_LOW);
    mGpioTemp->SetValue(PIN_LOW);
    mGpioWeekday->SetValue(PIN_LOW);
}

/*! \brief Destructor
    \note Sets all GPIO lamps to low and deletes the GPIO objects.
*/
cNixieLamps::~cNixieLamps()
{
    mGpioHms->SetValue(PIN_LOW);
    mGpioTemp->SetValue(PIN_LOW);
    mGpioWeekday->SetValue(PIN_LOW);
    delete mGpioHms;
    delete mGpioTemp;
    delete mGpioWeekday;
}

/*! \brief Sets the state of a lamp.
    \param _type The type of lamp to set (HMS, Temperature, Weekday, or All).
    \param _on True to turn the lamp on, false to turn it off. Pretty obvious, right?
    \note This function sets the specified lamp to the given state.
*/
void cNixieLamps::SetLamp(enumLampType _type, bool _on) const {
    PIN_VALUE pinValue = _on ? PIN_HIGH : PIN_LOW;
    switch(_type)
    {
        case enuLampHms:        mGpioHms->SetValue(pinValue);       break;
        case enuLampTemp:       mGpioTemp->SetValue(pinValue);      break;
        case enuLampWeekday:    mGpioWeekday->SetValue(pinValue);   break;
        case enumLampAll:
            mGpioHms->SetValue(pinValue);
            mGpioTemp->SetValue(pinValue);
            mGpioWeekday->SetValue(pinValue);
            break;
    }
}

//eof lamps.cpp