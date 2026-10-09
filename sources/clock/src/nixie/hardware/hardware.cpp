/*! \file hardware.cpp */
#include "hardware.h"
#include "../config.h"
#include <cstdio>
#include <unistd.h>
#include "../../logger/logger.h"
#include "vu.h"
#include "dimmer.h"

/*! \brief Constructor
    \note Initializes the GPIO pins, vu, dimmer, lamps, numbers, and RGB hardware.
    \note The vu and dimmer are initialized with the minimum and maximum values from the global configuration.
    \note The lamps are initialized with the default values.
    \note The numbers are initialized with the default values.
    \note The RGB hardware is initialized with the default values.
    \note Also initializes the Raspberry GPIO driver
*/
cNixieHardware::cNixieHardware(): mTxtTime{} {
    gpio_init();
    mNixieVu = init_vu(gNixieConfig.vu_min, gNixieConfig.vu_max);
    mNixieDimmer = init_dimmer(gNixieConfig.brightness);
    mNixieLamps = new cNixieLamps();
    mNixieNumbers = init_numbers();
    mRpiRgbh = init_rgb();
}

/*! \brief Destructor
    \note Deinitializes the numbers, RGB hardware, vu, dimmer, and lamps.
    \note Also deinitializes the Raspberry GPIO driver
*/
cNixieHardware::~cNixieHardware()
{
    deinit_numbers();   mNixieNumbers = nullptr;
    deinit_rgb();       mRpiRgbh = nullptr;
    deinit_vu();        mNixieVu = nullptr;
    deinit_dimmer();    mNixieDimmer = nullptr;
    delete mNixieLamps; mNixieLamps = nullptr;
    gpio_end();
}

/*! \brief Shows a number on the Nixie display.
    \param _txt The text to display on the Nixie display.
    \param _animtype The animation type to use when displaying the number.
    \param _u32delay The delay in milliseconds before the next number is displayed.
    \note If mNixieNumbers is not initialized, this function does nothing.
*/
void cNixieHardware::ShowNumber(const char *_txt, enumNumberAnim _animtype, uint32_t _u32delay) const {
    if(mNixieNumbers)
        mNixieNumbers->SetValue(_txt, _animtype, _u32delay);
}

/*! \brief Shows a number on the Nixie RGB leds mounted below each tube.
    \param _r: The red component of the RGB color.
    \param _g: The green component of the RGB color.
    \param _b: The blue component of the RGB color.
    \note If mRpiRgbh is not initialized, this function does nothing.
*/
void cNixieHardware::SetRgb(uint8_t _r, uint8_t _g, uint8_t _b) const {
    if(mRpiRgbh){
        mRpiRgbh->SetRgb(_r, _g, _b);
    }
} 

/*! \brief Sets the dimmer percentage. Dimmer is the global brightness for all the tubes.
    \details This function sets the dimmer percentage for the Nixie display.
    \param _percent The percentage to set the dimmer to (0-100).
    \note If mNixieDimmer is not initialized, this function does nothing.
    \note The global brightness is controlled by a PWM signal over the high voltage line.
    \warning Brightness is controlled by a two transistor switch, so the values are inverted. 
*/
void cNixieHardware::SetDimmerPercent(uint8_t _percent) const {
    if(mNixieDimmer){
        mNixieDimmer->SetPercent(_percent);
    }
}

/*! \brief Sets the vu percentage. Vu is the bargraph in the front of the box..
    \param _percent The percentage to set the vu to (0-100).
    \note If mNixieVu is not initialized, this function does nothing.
*/
void cNixieHardware::SetVuPercent(uint8_t _percent) const {
    if(mNixieVu){
        mNixieVu->SetPercent(_percent);
    }
}

/*! \brief Turns the bargraph really off
    \note SetVuPercent(0) lands on the calibrated vu_min, the bottom of the scale, which still
    \note glows a little. This writes a physical zero instead.
*/
void cNixieHardware::SetVuOff() const {
    if(mNixieVu)
        mNixieVu->SetPhysicalZero();
}

/*! \brief Sets a Lamp
    \param _temperature The temperature in degrees Celsius to set the vu to.
    \note If mNixieVu is not initialized, this function does nothing.
    \note The temperature is converted to a percentage (0-100) based on the maximum temperature of 45°C.
*/
void cNixieHardware::SetLamp(enumLampType _type, bool _on) const {
    if(mNixieLamps)
        mNixieLamps->SetLamp(_type, _on);
}

/*! \brief Sets a modulation module to the VU, dimmer or RGB outputs
    \param _type The type of hardware to set the modulation for (VU, dimmer, or RGB).
    \param _modulationType The type of modulation to set (ramp, sinusoidal, flash, etc.).
    \param _period The period of the modulation in milliseconds (default is 1000ms).
    \note If the specified hardware type is not initialized, this function does nothing.
    \note This function allows for dynamic modulation of the VU, dimmer or RGB outputs.
    \note The modulation can be used to create effects like fading, flashing, or sinusoidal modulation.
*/
void cNixieHardware::SetModulation(enumHardwareType _type, enuModulationType _modulationType, uint32_t _period) const {
    switch(_type)
    {
        case enumHardwareTypeVu:
            if(mNixieVu) {
                mNixieVu->SetModulator(_modulationType);
                mNixieVu->SetModulationPeriod(_period);
            }
            break;
        case enumHardwareTypeDimmer:
            if(mNixieDimmer) {
                mNixieDimmer->SetModulator(_modulationType);
                mNixieDimmer->SetModulationPeriod(_period);
            }
            break;
        case enumHardwareTypeRgb:
            if(mRpiRgbh){
                mRpiRgbh->SetModulator(_modulationType);
                mRpiRgbh->SetModulationPeriod(_period);
            }
            break;
    }
}

/*! \brief Sets the minimum value for the vu bargraph.
    \param _u8min The minimum value to set for the vu bargraph
    \note This function sets the vu bargraph to the minimum value and turns off all other outputs.
    \note This is used to adjust the bargraph to a specific minimum level, indicated by 0°C in the display.
*/
void cNixieHardware::SetVuMin(uint8_t _u8min) const {
    SetAllOff();
    if(mNixieVu)
        mNixieVu->SetMinimum(_u8min, 10000);
    SetVuPercent(0);
    SetModulation(enumHardwareTypeDimmer, enuModulationTypeNone);
    SetDimmerPercent(100);
}

/*! \brief Sets the maximum value for the vu bargraph.
    \param _u8max The maximum value to set for the vu bargraph
    \note This function sets the vu bargraph to the maximum value and turns off all other outputs.
    \note This is used to adjust the bargraph to a specific maximum level, indicated by 45°C in the display.
*/
void cNixieHardware::SetVuMax(uint8_t _u8max) const {
    SetAllOff();
    if(mNixieVu)
        mNixieVu->SetMaximum(_u8max, 10000);
    SetVuPercent(100);
    SetModulation(enumHardwareTypeDimmer, enuModulationTypeNone);
    SetDimmerPercent(100);
}

/*! \brief Gets the date or time as a formatted string.
    \param _type The type of date/time to get (time or date).
    \param _tm The tm structure containing the date/time information.
    \return A pointer to the formatted string.
    \note The formatted string is stored in mTxtTime.
*/
const char *cNixieHardware::GetDateTimeTxt(const enumDateTimeType _type, const tm &_tm)
{
    if(_type == enumDateTimeTypeTime)
        snprintf(mTxtTime, sizeof(mTxtTime), "%02d%02d%02d", _tm.tm_hour, _tm.tm_min, _tm.tm_sec);
    else
        snprintf(mTxtTime, sizeof(mTxtTime), "%02d%02d%02d", _tm.tm_mday, _tm.tm_mon + 1, _tm.tm_year % 100);
    return mTxtTime;  
}

/*! \brief Shows the current time on the Nixie display.
    \param _tm The tm structure containing the current time.
    \param _animtype The animation type to use when displaying the time.
    \param _u32delay The delay in milliseconds before the next number is displayed.
    \note Lamps are set as expeted
*/
void cNixieHardware::ShowTime(const tm &_tm, enumNumberAnim _animtype, uint32_t _u32delay)
{
    if(mNixieLamps)
    {
        mNixieLamps->SetLamp(enuLampHms, true);
        mNixieLamps->SetLamp(enuLampTemp, true);
        mNixieLamps->SetLamp(enuLampWeekday, false);
    }
    ShowNumber(GetDateTimeTxt(enumDateTimeTypeTime,_tm), _animtype, _u32delay);
}

/*! \brief Shows the current date on the Nixie display.
    \param _tm The tm structure containing the current date.
    \param _animtype The animation type to use when displaying the date.
    \param _u32delay The delay in milliseconds before the next number is displayed.
    \note Lamps are set as expected
*/
void cNixieHardware::ShowDate(const tm &_tm, enumNumberAnim _animtype, uint32_t _u32delay)
{
    if(mNixieLamps)
    {
        mNixieLamps->SetLamp(enuLampHms, false);
        mNixieLamps->SetLamp(enuLampTemp, false);
        mNixieLamps->SetLamp(enuLampWeekday, true);
    }
    ShowNumber(GetDateTimeTxt(enumDateTimeTypeDate, _tm), _animtype, _u32delay);
    mNixieVu->SetPercent(GetWeekDayPercent(_tm));
}

/*! \brief Shows the temperature limits of the current forecast on the Nixie display.
    \param _min The minimum temperature limit to display.
    \param _max The maximum temperature limit to display.
    \param _animtype The animation type to use when displaying the limits.
    \param _u32delay The delay in milliseconds before the next number is displayed.
    \note Lamps are set as expected
*/
void cNixieHardware::ShowTempertaureLimits(uint8_t _min, uint8_t _max, enumNumberAnim _animtype, uint32_t _u32delay) const {
    if(mNixieLamps)
    {
        mNixieLamps->SetLamp(enuLampHms, false);
        mNixieLamps->SetLamp(enuLampTemp, true);
        mNixieLamps->SetLamp(enuLampWeekday, false);
    }
    char txt[7];
    snprintf(txt, sizeof(txt), "%02d  %02d", _min, _max);
    ShowNumber(txt, _animtype, _u32delay);
}

void cNixieHardware::ShowCloudPercentage(uint8_t _cloudPercentage, enumNumberAnim _animtype, uint32_t _u32delay) const{
    if(_cloudPercentage > 99)
        _cloudPercentage = 99; // Limit to 99%
    if(mNixieLamps)
    {
        mNixieLamps->SetLamp(enuLampHms, true);
        mNixieLamps->SetLamp(enuLampTemp, false);
        mNixieLamps->SetLamp(enuLampWeekday, false);
    }
    char txt[7];
    snprintf(txt, sizeof(txt), "  %02d  ", _cloudPercentage);
    ShowNumber(txt, _animtype, _u32delay);
}

/*! \brief Lights a single digit on a single tube, blanking all the others.
    \param _position The tube to light, 0 being the leftmost and 5 the seconds units.
    \param _digit The digit to show, 0 to 9.
    \note Used by the cathode regeneration routine, which cycles 0..9 on one tube at a time.
    \note Out of range arguments are ignored so a bad caller cannot blank the display.
*/
void cNixieHardware::ShowSingleDigit(uint8_t _position, uint8_t _digit) const
{
    if(_position > 5 || _digit > 9)
        return;
    char txt[7] = "      ";
    txt[_position] = static_cast<char>('0' + _digit);
    ShowNumber(txt, enumNumberAnimNone);
}

/*! \brief Sets all outputs to off state.
    \note This function turns off the vu, dimmer, RGB, numbers, and lamps.
    \note It is used to reset the hardware to a known state.
*/
void cNixieHardware::SetAllOff() const {
    mNixieVu->SetPercent(0);
    mRpiRgbh->SetRgb(0, 0, 0);    
    mNixieNumbers->SetValue("      ", enumNumberAnimNone, 0);
    mNixieLamps->SetLamp(enuLampHms, false);
    mNixieLamps->SetLamp(enuLampTemp, false);
    mNixieLamps->SetLamp(enuLampWeekday, false); 
}

/*! \brief Sets the maximum brightness for the Nixie display.
    \param _u8max The maximum brightness value to set (0-100).
    \param _silent If true, does not update the vu or lamps.
    \note This function sets the maximum brightness for the Nixie display and updates the vu and lamps accordingly.
    \note If _silent is true, it does not update the vu or lamps.
    \note Silent, only the ceiling moves: the dimmer stays where it was, off if the clock is asleep.
    \note It used to be driven to 100% as well, so the ambient light dimming or restoring the tubes
    \note in the middle of the night lit a sleeping clock, frozen on the time it fell asleep at.
*/
void cNixieHardware::SetMaxBrightness(uint8_t _u8max, bool _silent) const {
    glogger->debug("Setting max brightness to {:d}", _u8max);
    mNixieDimmer->SetLimits(_u8max);
    if(_silent)
        return;

    mNixieDimmer->SetPercent(100);
    SetModulation(enumHardwareTypeDimmer, enuModulationTypeNone);
    SetModulation(enumHardwareTypeVu, enuModulationTypeNone);
    mNixieVu->SetPercent(100);  
    mNixieNumbers->SetValue("000000", enumNumberAnimNone, 0);
    mNixieLamps->SetLamp(enuLampHms, true);
    mNixieLamps->SetLamp(enuLampTemp, true);
    mNixieLamps->SetLamp(enuLampWeekday, true);
}

/*! \brief Given a week day and hour, gets the percentage of the week
    \param _tm The tm structure containing the week day and hour
    \return The percentage of the week as a uint8_t value (0-100)
*/
uint8_t cNixieHardware::GetWeekDayPercent(const tm &_tm)
{
    double totalhours = _tm.tm_wday * 24 + _tm.tm_hour;
    totalhours /= 1.68;
    return totalhours;
}

/*! \brief Given a temperature, get the percentage to display in bargraph
    \param _temperature The temperature in degrees Celsius
    \return The percentage of the temperature as a uint8_t value (0-100)
*/
uint8_t cNixieHardware::GetTemperaturePercent(double _temperature)
{
    if(_temperature >= 45.0) return 100;
    if(_temperature <= 0.0)  return 0;

    _temperature *= 100.0;
    _temperature /= 45.0;
    return static_cast<uint8_t>(_temperature);
}

/*! \brief Sets the vu bargraph to the temperature.
    \param _temperature The temperature in degrees Celsius to set the vu to.
    \note This function sets the vu bargraph to the temperature percentage and turns on the temperature lamp.
    \note The temperature is converted to a percentage (0-100) based on the maximum temperature of 45°C.
    \note The display does not show negative temperatures, so the minimum is 0°C.
*/
void cNixieHardware::SetVuTemperature(double _temperature)
{
    if(_temperature < 0.0)
        _temperature = 0.0;
    else if(_temperature > 45.0)
        _temperature = 45.0;
    mNixieVu->SetPercent(GetTemperaturePercent(_temperature));
    mNixieLamps->SetLamp(enuLampTemp, true);
    mNixieLamps->SetLamp(enuLampWeekday, false); 
}

//eof hardware.cpp