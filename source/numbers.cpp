/*! \file numbers.cpp */
#include "numbers.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>

/**
Class cNixieNumbers
    Manages the Nixie tube drivers that are connected to 74HC595 shift-registers
    Pins are defined on pins.h

    Each tube driver expects a 4 bit word to decode the digits.
    Some chips remaps invalid values (> 9), others blank the tubes.
    So, any invalid value is set to 10d (0x0a) that will be blank or mapped to '0'

    If you need to send something else, just assemble yourself an u32 value with the correct bits set and
    set them using ::SetValue(__u32)
    otherwise, just send the string to ::SetValue(const char*)

    The output of the shift registers may be buffered as they change or at the end of transmission
    You can define the behavior by setting a value to ::SetClockSpeed(__u32). If zero, data will be
    flushed to the drivers only at the end, making them change all at once
    If a value is set, data will be flushed as it is shifted, so the tubes will change its display as the bits go through,
    in a fancy animation.
**/

__u8 u8data, u8clock, u8bclock, u8reset;
void cNixieNumbers::Send2Chips(__u32 _u32value)
{
    __u32 u32index = 0x08;
    for(__u8 j=0;j<6;j++)
    {
        __u32 u32mask = u32index;
        for(__u8 i=0;i<4;i++)
        {
            mNeoGpioRaspi_Data->SetValue(u32mask & _u32value?PIN_HIGH:PIN_LOW);
            mNeoGpioRaspi_Clock->SetPulse(PIN_HIGH, 100);
            mNeoGpioRaspi_bClock->SetPulse(PIN_HIGH, mu32delayus);
            u32mask >>= 1;
        }
        u32index <<= 4;
    }
    mNeoGpioRaspi_bClock->SetPulse(PIN_HIGH, 100);
}

cNixieNumbers::cNixieNumbers(PIN_NAMES _clock, PIN_NAMES _data, PIN_NAMES _bufferclock, PIN_NAMES _reset)
{
    mNeoGpioRaspi_Clock  = new cNeoGpioRaspi(_clock, OUTPUT_PIN);
    mNeoGpioRaspi_Data   = new cNeoGpioRaspi(_data, OUTPUT_PIN);
    mNeoGpioRaspi_bClock = new cNeoGpioRaspi(_bufferclock, OUTPUT_PIN);
    mNeoGpioRaspi_Reset  = new cNeoGpioRaspi(_reset, OUTPUT_PIN);

    mNeoGpioRaspi_Clock->SetValue(PIN_LOW);
    mNeoGpioRaspi_bClock->SetValue(PIN_LOW);
    mNeoGpioRaspi_Reset->SetValue(PIN_HIGH);

    SetClockSpeed(0);
    Reset();

    mTxt[0] = 0;
}

cNixieNumbers::~cNixieNumbers()
{
    delete mNeoGpioRaspi_Clock;
    delete mNeoGpioRaspi_Data;
    delete mNeoGpioRaspi_bClock;
    delete mNeoGpioRaspi_Reset;
}

void cNixieNumbers::SetClockSpeed(__u32 _u32delayUs)
{
    mu32delayus = _u32delayUs;
}

void cNixieNumbers::Reset()
{
    mNeoGpioRaspi_Reset->SetPulse(PIN_LOW, 100);
}


void cNixieNumbers::SetValue(const char *_numbers, enumNumberAnim _animtype, __u32 _u32delay)
{
    if(_numbers == NULL)    return;

    if(_animtype == enumNumberAnimScroll)
        SetClockSpeed(_u32delay * 1000);
    else
        SetClockSpeed(0);

    if(_animtype == enumNumberAnimFlip && mTxt[0] != 0)
    {
        __u8 u8ok=0;
        do
        {
            u8ok=0;
            for(__u8 i=0;i<strlen(_numbers);i++)
            {
                if(mTxt[i] < _numbers[i])
                    (mTxt[i])++;
                else if(mTxt[i] > _numbers[i])
                    (mTxt[i])--;
                else
                    u8ok++;
            }
            SetValue(mTxt);
            usleep(_u32delay * 1000);
        }while(u8ok != strlen(_numbers));
    }
    else
    {
        SetValue(_numbers);
    }
}

// SetValue
// anything not a number willl be coded as 0x0a
// On 74141 or K155 if will blank the display
// On 74141A or K155A if will be mapped to '0'
void cNixieNumbers::SetValue(const char *_numbers)
{
    __u32 u32x = 0;
    if(_numbers == NULL || strlen(_numbers) > 6)
        return;
    for(__u8 i=0;i<strlen(_numbers);i++)
    {
        u32x <<= 4;
        __u32 aux;
        if(_numbers[i] >= '0' && _numbers[i] <= '9')
            aux = _numbers[i] - '0';
        else
            aux = 0x0a;
        u32x |= aux;
    }

    strncpy(mTxt, _numbers, 7);
    SetValue(u32x);
}

void cNixieNumbers::SetValue(__u32 _u32value)
{
    Reset();
    Send2Chips(_u32value);
}

//eof numbers.cpp
