/*! \file neogpio.cpp */
#include "gpio.h"
//#include "mmap.h"
#include <stdio.h>
#include <unistd.h>
#include <pigpio.h>

//struct bcm2835_peripheral gpio = {GPIO_BASE};
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
    gpioInitialise();
//    if(!gpio_inited)
//        map_peripheral(&gpio);
    gpio_inited = true;
}

void gpio_end()
{
    if(gpio_inited)
        gpioTerminate();
    //    unmap_peripheral(&gpio);
    gpio_inited = false;
}



cNeoGpioRaspi::cNeoGpioRaspi(PIN_NAMES gpio_pinname, PIN_DIRECTION _direction, PIN_PULLUPDOWN _pinpullupdown)
{
    if(!gpio_inited)
    {
        printf("**************************************\n");
        printf("***  MUST call gpio_init() before! ***\n");
        printf("**************************************\n");
    }
    mu32_gpiopin = (__u32)gpio_pinname;
    SetDirection(_direction, _pinpullupdown);

}

cNeoGpioRaspi::~cNeoGpioRaspi()
{
    SetDirection(INPUT_PIN, PULLUPDOWN_DISABLE);
}

bool cNeoGpioRaspi::SetDirection(PIN_DIRECTION _direction, PIN_PULLUPDOWN _pinpullupdown)
{
    switch(_direction)
    {
        case INPUT_PIN:
            gpioSetMode(mu32_gpiopin, PI_INPUT);
            switch(_pinpullupdown)
            {
                case PULLUPDOWN_DISABLE:    gpioSetPullUpDown(mu32_gpiopin, PI_PUD_OFF);  break;
                case PULLUPDOWN_DOWN:       gpioSetPullUpDown(mu32_gpiopin, PI_PUD_DOWN); break;
                case PULLUPDOWN_UP:         gpioSetPullUpDown(mu32_gpiopin, PI_PUD_UP);   break;
            }
            break;
        case OUTPUT_PIN:
            gpioSetMode(mu32_gpiopin, PI_OUTPUT);
            break;
    }
    return true;
}

bool cNeoGpioRaspi::SetValue(PIN_VALUE _value)
{
    gpioWrite(mu32_gpiopin, _value);
    return true;
}

bool cNeoGpioRaspi::SetPulse(PIN_VALUE _value, __u32 _u32usleeptime)
{
    if(_u32usleeptime == 0) return false;
    SetValue(_value);
    usleep((_u32usleeptime));
    SetValue(_value==PIN_HIGH?PIN_LOW:PIN_HIGH);
    return true;
}

bool cNeoGpioRaspi::GetValue()
{
    return gpioRead(mu32_gpiopin);
}


//eof gpio.cpp
