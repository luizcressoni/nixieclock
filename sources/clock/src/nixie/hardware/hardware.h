/*! \file hardware.h */
#pragma once
#include "lamps.h"
#include "numbers.h"
#include "rgb.h"


enum enumHardwareType
{
    enumHardwareTypeVu,
    enumHardwareTypeDimmer,
    enumHardwareTypeRgb
};

/*! \class cNixieHardware
    \brief This class handles the Nixie hardware operations, including displaying numbers, time, date, and controlling lamps and RGB LEDs.
    \note It uses cNixiePwm for PWM control, cNixieLamps for lamp control, cNixieNumbers for number display, and cRpiRgbh for RGB LED control.
    \note The class provides methods to set the VU meter, dimmer, RGB colors, and lamps.
    \note It also includes methods to show time and date in a formatted manner.
    \note The class supports different types of animations for number display, such as flipping and scrolling.
    \note The class can also set temperature limits and display them on the Nixie hardware.
    \note The class uses a thread-safe approach for PWM control and modulation.
    \note The class provides methods to set the minimum and maximum brightness for the VU meter
    \note and to set the maximum brightness for the dimmer.
    \note The class can also set the RGB color based on the current temperature.
*/
class cNixieHardware
{
    enum enumDateTimeType
    {
        enumDateTimeTypeTime,
        enumDateTimeTypeDate
    };
protected:
    cNixiePwm     *mNixieVu;
    cNixiePwm     *mNixieDimmer;
    cNixieLamps   *mNixieLamps;
    cNixieNumbers *mNixieNumbers;
    cRpiRgbh      *mRpiRgbh;
    char          mTxtTime[7];
    const char    *GetDateTimeTxt(enumDateTimeType _type, const tm &_tm);
    uint8_t       GetWeekDayPercent(const tm &_tm);
    uint8_t       GetTemperaturePercent(double _temperature);
public: 
    cNixieHardware();
    ~cNixieHardware();

    void ShowNumber(const char *_txt, enumNumberAnim _animtype = enumNumberAnimNone, uint32_t _u32delay = 0) const;
    void ShowTime(const tm &_tm, enumNumberAnim _animtype = enumNumberAnimNone, uint32_t _u32delay = 0);
    void ShowDate(const tm &_tm, enumNumberAnim _animtype = enumNumberAnimNone, uint32_t _u32delay = 0);
    void ShowTempertaureLimits(uint8_t _min, uint8_t _max, enumNumberAnim _animtype = enumNumberAnimNone, uint32_t _u32delay = 0) const;
    void ShowCloudPercentage(uint8_t _cloudPercentage, enumNumberAnim _animtype = enumNumberAnimNone, uint32_t _u32delay = 0) const;
    void ShowSingleDigit(uint8_t _position, uint8_t _digit) const;
    void SetRgb(uint8_t _r, uint8_t _g, uint8_t _b) const;
    void SetDimmerPercent(uint8_t _percent) const;
    void SetVuPercent(uint8_t _percent) const;
    void SetVuOff() const;
    void SetVuTemperature(double _temperature);
    void SetLamp(enumLampType _type, bool _on) const;
    void SetModulation(enumHardwareType _type, enuModulationType _modulationType, uint32_t _period = 1000) const;

    void SetVuMin(uint8_t _u8min) const;
    void SetVuMax(uint8_t _u8max) const;
    void SetMaxBrightness(uint8_t _u8max, bool _silent = true) const;


    void SetAllOff() const;
};
//eof hardware.h