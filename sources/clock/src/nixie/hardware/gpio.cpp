/*! \file gpio.cpp */
#include "gpio.h"
#include <unistd.h>
#include <pigpio.h>

bool gpio_inited = false;

/** GPIO registers **/
#define GPIO_FSEL(n)    *(gpio.addr + ((n)/10))         //! Select
#define GPIO_SET(n)     *(gpio.addr + 0x07 + (n/32))    //! Set pin
#define GPIO_CLR(n)     *(gpio.addr + 0x0A + (n/32))    //! Clear pin
#define GPIO_LEV(n)     *(gpio.addr + 0x0D + (n/32))    //! Read pin level
#define GPIO_EDS        *(gpio.addr + 0x10)             //! Edge detection status
#define GPIO_REN        *(gpio.addr + 0x13)             //! Rising Edge Enable
#define GPIO_FEN        *(gpio.addr + 0x16)             //! Falling Edge Enable
#define GPIO_PUD        *(gpio.addr + 0x25)             //! Pull up/down enable
#define GPIO_PUDCLK(n)  *(gpio.addr + 0x26 + (n/32))    //! Pull up/down clock enable

void gpio_init()
{
    //pigpio installs a handler for every signal from 1 to 63 unless told not to, and its
    //default action on a termination signal is to call gpioTerminate() and exit() behind our
    //back, racing the orderly shutdown in main(). We handle those ourselves, in one place.
    gpioCfgSetInternals(gpioCfgGetInternals() | PI_CFG_NOSIGHANDLER);
    gpioInitialise();
    gpio_inited = true;
}

void gpio_end()
{
    if(gpio_inited)
        gpioTerminate();
    gpio_inited = false;
}


/*! \brief Constructor
    \param gpio_pinname The GPIO pin name to be used.
    \param _direction The direction of the GPIO pin (INPUT_PIN or OUTPUT_PIN).
    \param _pinpullupdown The pull-up/down configuration for the GPIO pin (PULLUPDOWN_DISABLE, PULLUPDOWN_DOWN, PULLUPDOWN_UP).
    \note This constructor initializes the GPIO pin and sets its direction and pull-up/down configuration.
    \note The first instance also initializes the GPIO library using gpio_init().
*/
cGpioRaspi::cGpioRaspi(PIN_NAMES gpio_pinname, PIN_DIRECTION _direction, PIN_PULLUPDOWN _pinpullupdown)
{
    if(!gpio_inited)
        gpio_init();

    mu32_gpiopin = static_cast<uint32_t>(gpio_pinname);
    SetDirection(_direction, _pinpullupdown);
}

/*! \brief Destructor
    \note This destructor sets the GPIO pin direction to INPUT_PIN with PULLUPDOWN_DISABLE.
    \note It is called when the cGpioRaspi object is destroyed.
*/
cGpioRaspi::~cGpioRaspi()
{
    SetDirection(INPUT_PIN, PULLUPDOWN_DISABLE);
}

/*! \brief Set the direction and pull-up/down configuration of the GPIO pin.
    \param _direction The direction of the GPIO pin (INPUT_PIN or OUTPUT_PIN).
    \param _pinpullupdown The pull-up/down configuration for the GPIO pin (PULLUPDOWN_DISABLE, PULLUPDOWN_DOWN, PULLUPDOWN_UP).
    \return true 
    \note This method sets the direction and pull-up/down configuration of the GPIO pin.
*/
bool cGpioRaspi::SetDirection(PIN_DIRECTION _direction, PIN_PULLUPDOWN _pinpullupdown)
{
    switch(_direction)
    {
        case INPUT_PIN:
            gpioSetMode(mu32_gpiopin, PI_INPUT);
            gpioSetPullUpDown(mu32_gpiopin, _pinpullupdown);
            break;
        case OUTPUT_PIN:
            gpioSetMode(mu32_gpiopin, PI_OUTPUT);
            break;
    }
    return true;
}

/*! \brief Set the value of the GPIO pin.
    \param _value The value to set for the GPIO pin (PIN_LOW or PIN_HIGH).
    \return true
    \note This method sets the value of the GPIO pin to either low or high.
*/
bool cGpioRaspi::SetValue(PIN_VALUE _value)
{
    gpioWrite(mu32_gpiopin, _value);
    return true;
}

/*! \brief Set a pulse on the GPIO pin.
    \param _value The value to set for the GPIO pin (PIN_LOW or PIN_HIGH).
    \param _u32usleeptime The duration of the pulse in microseconds.
    \return true
    \note This method sets the GPIO pin to the specified value, waits for the specified duration, and then toggles the pin value.
*/
bool cGpioRaspi::SetPulse(PIN_VALUE _value, uint32_t _u32usleeptime)
{
    if(_u32usleeptime == 0) return false;
    SetValue(_value);
    usleep((_u32usleeptime));
    SetValue(_value==PIN_HIGH?PIN_LOW:PIN_HIGH);
    return true;
}

/*! \brief Get the value of the GPIO pin.
    \return Returns true if the GPIO pin is high, false if it is low.
    \note This method reads the current value of the GPIO pin.
*/
bool cGpioRaspi::GetValue()
{
    return gpioRead(mu32_gpiopin);
}


//eof gpio.cpp
