/*! \file numbers.h */
#pragma once
#include "gpio.h"



enum enumNumberAnim
{
    enumNumberAnimNone,         //just replace
    enumNumberAnimFlip,         //flip digit by digit one unit at a time until match
    enumNumberAnimScroll        //scroll from left to right
};

class cNixieNumbers
{
  protected:
    cGpioRaspi   *mGpioRaspi_Clock,
                 *mGpioRaspi_Data,
                 *mGpioRaspi_bClock,
                 *mGpioRaspi_Reset;
    void        Send2Chips(uint32_t _u32value) const;
    uint32_t    mu32delayus{};
    char        mTxt[8]{};
  public:
    cNixieNumbers(PIN_NAMES _clock, PIN_NAMES _data, PIN_NAMES _bufferclock, PIN_NAMES _reset);
    ~cNixieNumbers();

    void    Reset() const;
    void    SetClockSpeed(uint32_t _u32delayUs);
    void    SetValue(const char *_numbers, enumNumberAnim _animtype, uint32_t _u32delay);
    void    SetValue(const char *_numbers);
    void    SetValue(uint32_t _u32value) const;
};


cNixieNumbers *init_numbers();
void deinit_numbers();
//eof numbers.h