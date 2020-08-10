/*! \file pins.h */

#define PIN_PWM_T               GPIO_24_GEN5        //temperature & weekday tube PWM

#define PIN_PWM_LAMPS           GPIO_23_GEN4        //Nixie High Voltage dimmer
#define PIN_LAMP_TEMP           GPIO_25_GEN6        //weekday indicator lamp
#define PIN_LAMP_WDAY           GPIO_08_CE0         //temperature indicator lamp
#define PIN_LAMP_HMS            GPIO_07_CE1         //hour-minute-second dots
//#define PIN_RESET               GPIO_12             //configuration reset switch

#define PIN_RGB_R               GPIO_16             //RGB channel red PWM
#define PIN_RGB_G               GPIO_20             //RGB channel green PWM
#define PIN_RGB_B               GPIO_26             //RGB channel blue PWM

#define PIN_N_DATA              GPIO_17_GEN0        //74HC595 Data line - pin 14
#define PIN_N_CLOCK             GPIO_27_GEN2        //shift Clock line - pin 11
#define PIN_N_BCLOCK            GPIO_05             //latch clock - pin 12
#define PIN_N_RESET             GPIO_22_GEN3        //shift register Reset - pin 10
//eof pins.h
