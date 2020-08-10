/*! \file cNeoPwm.h */
#pragma once

#include "gpio.h"


/**
    PWM Pins:
        PWM0: 12,4(Alt0) 18,2(Alt5) 40,4(Alt0)            52,5(Alt1)
        PWM1: 13,4(Alt0) 19,2(Alt5) 41,4(Alt0) 45,4(Alt0) 53,5(Alt1)

        Notes:
        1) Pin 18 is the only one available on all platforms, and
             it is the one used by the I2S audio interface.
             Pins 12 and 13 might be better choices on an A+, B+ or Pi2.
          2) The onboard analogue audio output uses both PWM channels.
          3) So be careful mixing audio and PWM.
          4) Currently the clock must have been enabled and configured
             by other means.
*/


class cNeoPwm
{
  protected:

  public:
        cNeoPwm();
        virtual ~cNeoPwm();
        virtual void    SetPwmPin(__u8 _u8channel, PIN_NAMES _pin);
        virtual void    SetPwmMax(__u8 _u8channel, __u32 _u32maxvalue);
        virtual void    SetDutyCycleValue(__u8 _u8channel, __u32 _u32value);
        virtual void    SetDutyCyclePercent(__u8 _u8channel, double _percent);
};

//eof cNeoPwm.h
