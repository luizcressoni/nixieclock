/*! \file lamps.h */
#pragma once
#include "gpio.h"

enum enumLampType
{
    enuLampHms = 0,
    enuLampTemp,
    enuLampWeekday,
    enumLampAll
};

class cNixieLamps
{
 protected:
    cGpioRaspi *mGpioHms;
    cGpioRaspi *mGpioTemp;
    cGpioRaspi *mGpioWeekday;
public:
    cNixieLamps();
    ~cNixieLamps();

    void SetLamp(enumLampType _type, bool _on) const;
};