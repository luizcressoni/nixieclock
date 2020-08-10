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
    cNeoGpioRaspi   *mNeoGpioRaspi_Clock,
                    *mNeoGpioRaspi_Data,
                    *mNeoGpioRaspi_bClock,
                    *mNeoGpioRaspi_Reset;
    virtual void    Send2Chips(__u32 _u32value);
    __u32           mu32delayus;
    char            mTxt[8];
  public:
    cNixieNumbers(PIN_NAMES _clock, PIN_NAMES _data, PIN_NAMES _bufferclock, PIN_NAMES _reset);
    virtual ~cNixieNumbers();

    virtual void    Reset();
    virtual void    SetClockSpeed(__u32 _u32delayUs);
    virtual void    SetValue(const char *_numbers, enumNumberAnim _animtype, __u32 _u32delay);
    virtual void    SetValue(const char *_numbers);
    virtual void    SetValue(__u32 _u32value);
};


//eof numbers.h
