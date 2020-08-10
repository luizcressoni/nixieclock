/*! \file gpio.h */
#pragma once
#include <asm/types.h>

/********** Conectores Raspberry Pi B *********************************

                          +-------+
                  3V3 ---- 01   02 ---- 5V
     (SDA1 I2C)GPIO02 ---- 03   04 ---- 5V
     (SCL1 I2C)GPIO03 ---- 05   06 ---- GND
    (GPIO_GCLK)GPIO04 ---- 07   08 ---- GPIO14(TXD0)
                  GND ---- 09   10 ---- GPIO15(RXD0)
    (GPIO_GEN0)GPIO17 ---- 11   12 ---- GPIO18(GPIO_GEN1)
    (GPIO_GEN2)GPIO27 ---- 13   14 ---- GND
    (GPIO_GEN3)GPIO22 ---- 15   16 ---- GPIO23(GPIO_GEN4)
                  3V3 ---- 17   18 ---- GPIO24(GPIO_GEN5)
     (SPI_MOSI)GPIO10 ---- 19   20 ---- GND
     (SPI MISO)GPIO09 ---- 21   22 ---- GPIO25(GPIO_GEN6)
      (SPI_CLK)GPIO11 ---- 23   24 ---- GPIO08(SPI_CE0_N)
                  GND ---- 25   26 ---- GPIO07(SPI_CE1_N)
 (I2C ID EEPROM)ID_SD ---- 27   28 ---- ID_SC(I2C ID EEPROM)
               GPIO05 ---- 29   30 ---- GND
               GPIO06 ---- 31   32 ---- GPIO12
               GPIO13 ---- 33   34 ---- GND
               GPIO19 ---- 35   36 ---- GPIO16
               GPIO26 ---- 37   38 ---- GPIO20
                  GND ---- 39   40 ---- GPIO21
                          +-------+

*******************************************************************/

/******** Constantes e enumeradores *************/

enum PIN_NAMES          //!< nomes dos pinos de GPIO, conforme manual
{
    GPIO_02_SDA1    = 2,
    GPIO_03_SCL1    = 3,
    GPIO_04_GCLK    = 4,
    GPIO_05         = 5,
    GPIO_06         = 6,
    GPIO_07_CE1     = 7,
    GPIO_08_CE0     = 8,
    GPIO_09_MIS0    = 9,
    GPIO_10_M0SI    = 10,
    GPIO_11_CLK     = 11,
    GPIO_12         = 12,
    GPIO_13         = 13,
    GPIO_14_TXD     = 14,
    GPIO_15_RXD     = 15,
    GPIO_16         = 16,
    GPIO_17_GEN0    = 17,
    GPIO_18_GEN1    = 18,
    GPIO_19         = 19,
    GPIO_20         = 20,
    GPIO_21         = 21,
    GPIO_22_GEN3    = 22,
    GPIO_23_GEN4    = 23,
    GPIO_24_GEN5    = 24,
    GPIO_25_GEN6    = 25,
    GPIO_26         = 26,
    GPIO_27_GEN2    = 27,

    GPIO_NONE       = 0
};



enum PIN_DIRECTION{     //!< Definições de direção para sinais dos pinos
	INPUT_PIN=0,
	OUTPUT_PIN=1
};

enum PIN_VALUE{         //!< Definições de valores dos pinos
	PIN_LOW=0,
	PIN_HIGH=1
};

enum PIN_PULLUPDOWN
{
    PULLUPDOWN_DISABLE = 0,
    PULLUPDOWN_DOWN    = 1,
    PULLUPDOWN_UP      = 2

};


void gpio_init();
void gpio_end();

/**************************************************************************/
/*! \class      cNeoGpioRaspi
    \brief      Implementa controle de pinos GPIO da Raspberry usando lib pigpio
    \author     Neocortex - Luiz
    \date       2014.02
    \version    1.0
*/
class cNeoGpioRaspi
{
 protected:
    __u32   mu32_gpiopin;
 public:
    cNeoGpioRaspi(PIN_NAMES gpio_pinname, PIN_DIRECTION _direction, PIN_PULLUPDOWN _pinpullupdown = PULLUPDOWN_DISABLE);
    virtual ~cNeoGpioRaspi();

    virtual bool SetDirection(PIN_DIRECTION _direction, PIN_PULLUPDOWN _pinpullupdown = PULLUPDOWN_DISABLE);
    virtual bool SetValue(PIN_VALUE _value);
    virtual bool SetPulse(PIN_VALUE _value, __u32 _u32usleeptime);
    virtual bool GetValue();
};



//eof gpio.h
