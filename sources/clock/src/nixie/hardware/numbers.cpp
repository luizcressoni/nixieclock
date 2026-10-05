/*! \file numbers.cpp */
#include "numbers.h"
#include <cstring>
#include <unistd.h>
#include "pins.h"

/**
Class cNixieNumbers
    Manages the Nixie tube drivers that are connected to 74HC595 shift-registers
    Pins are defined on pins.h

    Each tube driver expects a 4 bit word to decode the digits.
    Some chips remaps invalid values (> 9), others blank the tubes.
    So, any invalid value is set to 10d (0x0a) that will be blank or mapped to '0'

    If you need to send something else, just assemble yourself an u32 value with the correct bits set and
    set them using ::SetValue(uint32_t _u32value)
    otherwise, just send the string to ::SetValue(const char*)

    The output of the shift registers may be buffered as they change or at the end of transmission
    You can define the behavior by setting a value to ::SetClockSpeed(__u32). If zero, data will be
    flushed to the drivers only at the end, making them change all at once
    If a value is set, data will be flushed as it is shifted, so the tubes will change its display as the bits go through,
    in a fancy animation.
**/
cNixieNumbers *gNixieNumbers = nullptr;

cNixieNumbers *init_numbers()
{
    gNixieNumbers = new cNixieNumbers(PIN_N_CLOCK, PIN_N_DATA, PIN_N_BCLOCK, PIN_N_RESET);
    uint32_t value = 0;
    gNixieNumbers->SetValue(value); //set numbers to 0
    return gNixieNumbers;
}

void deinit_numbers()
{
    if(gNixieNumbers != nullptr)
    {
        gNixieNumbers->SetValue("      "); //clear numbers
        delete gNixieNumbers;
        gNixieNumbers = nullptr;
    }

}

void cNixieNumbers::Send2Chips(uint32_t _u32value) const {
    uint32_t u32index = 0x08;
    for(int j=0;j<6;j++)
    {
        uint32_t u32mask = u32index;
        for(int i=0;i<4;i++)
        {
            mGpioRaspi_Data->SetValue(u32mask & _u32value?PIN_HIGH:PIN_LOW);
            mGpioRaspi_Clock->SetPulse(PIN_HIGH, 100);
            mGpioRaspi_bClock->SetPulse(PIN_HIGH, mu32delayus);
            u32mask >>= 1;
        }
        u32index <<= 4;
    }
    mGpioRaspi_bClock->SetPulse(PIN_HIGH, 100);
}

cNixieNumbers::cNixieNumbers(PIN_NAMES _clock, PIN_NAMES _data, PIN_NAMES _bufferclock, PIN_NAMES _reset)
{
    mGpioRaspi_Clock  = new cGpioRaspi(_clock, OUTPUT_PIN);
    mGpioRaspi_Data   = new cGpioRaspi(_data, OUTPUT_PIN);
    mGpioRaspi_bClock = new cGpioRaspi(_bufferclock, OUTPUT_PIN);
    mGpioRaspi_Reset  = new cGpioRaspi(_reset, OUTPUT_PIN);

    mGpioRaspi_Clock->SetValue(PIN_LOW);
    mGpioRaspi_bClock->SetValue(PIN_LOW);
    mGpioRaspi_Reset->SetValue(PIN_HIGH);

    SetClockSpeed(0);
    Reset();

    mTxt[0] = 0;
}

cNixieNumbers::~cNixieNumbers()
{
    delete mGpioRaspi_Clock;
    delete mGpioRaspi_Data;
    delete mGpioRaspi_bClock;
    delete mGpioRaspi_Reset;
}

void cNixieNumbers::SetClockSpeed(uint32_t _u32delayUs)
{
    mu32delayus = _u32delayUs;
}

void cNixieNumbers::Reset() const {
    mGpioRaspi_Reset->SetPulse(PIN_LOW, 100);
}


void cNixieNumbers::SetValue(const char *_numbers, enumNumberAnim _animtype, uint32_t _u32delay)
{
    if(_numbers == nullptr)    return;

    if(_animtype == enumNumberAnimScroll)
        SetClockSpeed(_u32delay * 1000);
    else
        SetClockSpeed(0);

    if(_animtype == enumNumberAnimFlip && mTxt[0] != 0)
    {
        int u8ok=0;
        do
        {
            u8ok=0;
            for(int i=0;i<strlen(_numbers);i++)
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
// anything not a number will be coded as 0x0a
// On 74141 or K155 if will blank the display
// On 74141A or K155A if will be mapped to '0'
void cNixieNumbers::SetValue(const char *_numbers)
{
    uint32_t u32x = 0;
    if(_numbers == nullptr || strlen(_numbers) > 6)
        return;
    for(int i=0;i<strlen(_numbers);i++)
    {
        u32x <<= 4;
        uint32_t aux;
        if(_numbers[i] >= '0' && _numbers[i] <= '9')
            aux = _numbers[i] - '0';
        else
            aux = 0x0a;
        u32x |= aux;
    }

    strncpy(mTxt, _numbers, 7);
    SetValue(u32x);
}

void cNixieNumbers::SetValue(uint32_t _u32value) const {
    Reset();
    Send2Chips(_u32value);
}

//eof numbers.cpp